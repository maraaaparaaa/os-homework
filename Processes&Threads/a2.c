#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "a2_helper.h"

typedef struct {
    int process;
    int threadnr;
    sem_t* sem1;
    sem_t* sem2;
    sem_t* sem_start;
    sem_t* sem_end;
    pthread_mutex_t* lock;
    pthread_cond_t* cond;
}TH_STRUCT;


int active_threads = 0;
int th10_ended = 0;

void* thread_function23(void* arg)   //Sincronizarea threadurilor din acelasi proces
{
    TH_STRUCT* params = (TH_STRUCT*)arg;

    if(params->threadnr == 4){
        info(BEGIN, params->process, params->threadnr); //thread 4 start
        sem_post(params->sem_start);

        sem_wait(params->sem_end);

        info(END, params->process, params->threadnr); //thread end

    }
    else if(params->threadnr == 2){
        sem_wait(params->sem_start);
        info(BEGIN, params->process, params->threadnr); //thread 2 start

        info(END, params->process, params->threadnr); //thread end

        sem_post(params->sem_end);
    }
    else if(params->threadnr == 3){ //3.3         Sincronizarea threadurilor din procese diferite
    
        sem_wait(params->sem1); 
        info(BEGIN, params->process, params->threadnr); //thread 3 start
        info(END, params->process, params->threadnr); //thread end
        sem_post(params->sem2);
    }
    else{
        info(BEGIN, params->process, params->threadnr); //thread start
        info(END, params->process, params->threadnr); //thread end
    }

    return 0;
}

void* thread_function10(void* arg){ //functia pentru threadul 10

    TH_STRUCT* params = (TH_STRUCT*) arg;

    sem_wait(params->sem2);

    pthread_mutex_lock(params->lock);
    info(BEGIN, params->process, params->threadnr);

    info(END, params->process, params->threadnr);
    th10_ended = 1;
    pthread_cond_broadcast(params->cond);
    pthread_mutex_unlock(params->lock);

    return 0;
}

void* thread_function24(void* arg)  // Bariera pentru threaduri
{
    TH_STRUCT* params = (TH_STRUCT*)arg;

    sem_wait(params->sem1);

    if(active_threads > 5){
        info(BEGIN, params->process, params->threadnr);
        info(END, params->process, params->threadnr);
    }
    else{
        pthread_mutex_lock(params->lock);
        info(BEGIN, params->process, params->threadnr);
        active_threads++;
    

        if(active_threads == 5){
            sem_post(params->sem2);
        }

        while(th10_ended == 0){
            pthread_cond_wait(params->cond, params->lock);
        }

        info(END, params->process, params->threadnr);
        pthread_mutex_unlock(params->lock);
    }
    

    sem_post(params->sem1);
    
    return 0;

}

void* thread_function25(void* arg){ // Sincronizarea threadurilor din procese diferite
    TH_STRUCT* params = (TH_STRUCT*)arg;

    if(params->threadnr == 1){
        info(BEGIN, params->process, params->threadnr); //thread 4.1 start
        info(END, params->process, params->threadnr); //thread 4.1 end
        sem_post(params->sem1);
    }
    else if(params->threadnr == 3){
        sem_wait(params->sem2);
        info(BEGIN, params->process, params->threadnr); //thread 4.3 start
        info(END, params->process, params->threadnr); //thread 4.3 end
    }
    else{
        info(BEGIN, params->process, params->threadnr); //thread start
        info(END, params->process, params->threadnr); //thread end
    }

    return 0;
}


void create_threadsP3() {
    pthread_t tids[5];
    sem_t sem_start, sem_end;

    sem_init(&sem_start, 0, 0);
    sem_init(&sem_end, 0, 0);


    sem_t* sem = sem_open("/sem1", 0);
    sem_t* fsem = sem_open("/sem2", 0);

    TH_STRUCT params[5];
    for(int i=0;i<5;i++){
        params[i].process = 3;
        params[i].threadnr = i + 1;
        params[i].sem_start = &sem_start;
        params[i].sem_end = &sem_end;
        params[i].sem1 = sem;
        params[i].sem2 = fsem;
        pthread_create(&tids[i], NULL, thread_function23, &params[i]);
    }

    //wait for threads to finish 
    for(int i=0;i<5;i++){
        pthread_join(tids[i], NULL);
    }

    sem_close(sem);
    sem_close(fsem);

    sem_destroy(&sem_start);
    sem_destroy(&sem_end);
}

void create_threadsP4(){
    pthread_t tids[5];
    TH_STRUCT params[5];

    sem_t* sem = sem_open("/sem1", 0);
    sem_t* fsem = sem_open("/sem2", 0);

    for(int i=0;i<5;i++){
        params[i].process = 4;
        params[i].threadnr = i + 1;
        params[i].sem1 = sem;
        params[i].sem2 = fsem;
        pthread_create(&tids[i], NULL, thread_function25, &params[i]);
    }

    //wait for threads to finish 
    for(int i=0;i<5;i++){
        pthread_join(tids[i], NULL);
    }

    sem_close(sem);
    sem_close(fsem);

}

void create_threadsP6(){
    pthread_t tids[50];
    TH_STRUCT params[50];

    sem_t sem;
    sem_init(&sem, 0, 5);

    sem_t sem2;
    sem_init(&sem2, 0, 0);

    pthread_mutex_t lock;
    pthread_mutex_init(&lock, NULL);

    pthread_cond_t cond;
    pthread_cond_init(&cond, NULL);

    for(int i=0;i<50;i++){
        params[i].process = 6;
        params[i].threadnr = i + 1;
        params[i].sem1 = &sem;
        params[i].sem2 = &sem2;
        params[i].lock = &lock;
        params[i].cond = &cond;
        if(i==9) {
            pthread_create(&tids[i], NULL, thread_function10, &params[i]);
        }
        else{
            pthread_create(&tids[i], NULL, thread_function24, &params[i]);
        }
    }

    //wait for threads to finish 
    for(int i=0;i<50;i++){
        pthread_join(tids[i], NULL);
    }

    sem_destroy(&sem);
    sem_destroy(&sem2);
    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond);
}

int main(int argc, char** argv)
{
    init(); // tester initialization - only one time in the main process

    info(BEGIN, 1, 0); // inform the tester about main process' start

    sem_t* sem1 = sem_open("/sem1", O_CREAT, 0644, 0);
    sem_t* sem2 = sem_open("/sem2", O_CREAT, 0644, 0);

    pid_t pid = fork(); //P2

    if(pid == -1) {
        perror("Could not create the process.");
        return -1;
    }else if(pid == 0){
        // child - P2
        info(BEGIN, 2, 0);

        info(END, 2, 0);
        exit(0);
    }


    pid = fork(); //P3

    if(pid== -1){
        perror("Could not create the process.");
        return -1;
    }else if(pid == 0){
        // child - P3
        info(BEGIN, 3, 0);

        pid = fork(); //P8
        if(pid==0){
            //child - P8
            info(BEGIN, 8, 0);

            info(END, 8, 0);
            exit(0);
        }

        pid = fork(); //P9
        if(pid==0){
            //child - P9
            info(BEGIN, 9, 0);

            info(END, 9, 0);
            exit(0);
        }

        // P3 creates 5 threads
        create_threadsP3();

        while(wait(NULL)>0);
        info(END, 3, 0);
        exit(0);
    }

    pid = fork(); //P4

    if(pid == -1){
        perror("Could not create the process.");
        return -1;
    }else if(pid == 0){
        // child - P4
        info(BEGIN, 4, 0);

        pid = fork(); //P5
        if(pid==0){
            //child - P5
            info(BEGIN, 5, 0);

            info(END, 5, 0);
            exit(0);
        }

        create_threadsP4();

        while(wait(NULL)>0);

        info(END, 4, 0);
        exit(0);
    }

    pid = fork(); //P6

    if(pid == -1){
        perror("Could not create the process.");
        return -1;
    }else if(pid == 0){
        // child - P6
        info(BEGIN, 6, 0);

        // P6 creates 50 threads
        create_threadsP6();

        info(END, 6, 0);
        exit(0);
    }

    pid = fork(); //P7

    if(pid == -1){
        perror("Could not create the process.");
        return -1;
    }else if(pid == 0){
        // child - P7
        info(BEGIN, 7, 0);

        info(END, 7, 0);
        exit(0);
    }

    while(wait(NULL)>0);

    sem_close(sem1);
    sem_close(sem2);
    sem_unlink("sem1");
    sem_unlink("sem2");

    info(END, 1, 0); // inform the tester about main process' termination
    return 0;
}