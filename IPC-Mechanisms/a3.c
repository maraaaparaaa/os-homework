#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>

void send_string(int pipe, const char* msg){
    unsigned char msg_len = strlen(msg);
    // Write the length as 1 byte
    write(pipe, &msg_len, sizeof(msg_len));
    // Write the message itself
    write(pipe, msg, msg_len);
}

unsigned int read_number(int pipe){
    unsigned int value = 0;
    read(pipe, &value, sizeof(unsigned int));
    return value;
}

void read_string(int pipe, unsigned char** string) {
    unsigned char size = 0;

    if (read(pipe, &size, sizeof(unsigned char)) != sizeof(unsigned char)) {
        *string = NULL;
        return;
    }

    *string = (unsigned char*)malloc(size + 1);
    if (*string == NULL) {
        return;
    }

    if (read(pipe, *string, size) != size) {
        free(*string);
        *string = NULL;
        return;
    }
    (*string)[size] = '\0';
}


int create_shm(unsigned int size, int* shmFd, volatile char** sharedMem){
    
    *shmFd = shm_open("/cyEGECI", O_CREAT | O_RDWR, 0644);
    if(*shmFd < 0){
        return 0;
    }
    ftruncate(*shmFd, size);

    *sharedMem = (volatile char*)mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, *shmFd, 0);
    if(*sharedMem == (void*)-1){
        return 0;
    }

    return 1;
}
void close_shm(int shmFd, volatile char* sharedMem, unsigned int size){
    munmap((void*)sharedMem, size);
    sharedMem = NULL;   

    close(shmFd);
}

void calculate_section_size_offset(int section_no, unsigned int* section_size, unsigned int* section_offset, char* file_data){
    size_t section_entry_offset = 8 + 29 * (section_no - 1);

    memcpy(section_offset, file_data + section_entry_offset + 21, sizeof(int));
    memcpy(section_size, file_data + section_entry_offset + 25, sizeof(int));
}

void calculate_start_logical_offsets(unsigned int sections, unsigned int* section_start_logical_offsets, unsigned int* section_sizes){
    unsigned int logical_offset = 0;
    for(int i=0; i<sections; i++){
        section_start_logical_offsets[i] = logical_offset;
        
        int nr_blocks = section_sizes[i] / 5120; 
        if(section_sizes[i] % 5120 > 0){
            logical_offset += (nr_blocks + 1)* 5120;
        }else logical_offset += nr_blocks* 5120;
    }
}

int find_requested_logical_offset(unsigned int* section_start_logical_offsets, unsigned int* section_sizes, unsigned int logical_offset, volatile char* sharedMem, unsigned int no_bytes, unsigned int* section_offsets, char* file_data, unsigned int sections){
    int found = 0;
    for(int i=0; i<sections; i++){
        int start = section_start_logical_offsets[i];
        int end = start + section_sizes[i];
        if(logical_offset >= start && logical_offset + no_bytes <= end){
            found = 1;
            int offset_in_section = logical_offset - start;
            int file_offset = section_offsets[i] + offset_in_section;
            memcpy((void*)sharedMem, file_data + file_offset, no_bytes);
            break;
        }
    }
    return found;
}

int main()
{
    int req_pipe = -1;
    int resp_pipe = -1;

    int shmFd = -1;
    volatile char* sharedMem = NULL;
    unsigned int shm_size = 0;

    int fd = -1;
    char* file_data = NULL;
    unsigned char* path = NULL;
    off_t file_size = 0;

    unsigned int sections = 0; // number of sections from file

    //create pipe 
    if(mkfifo("RESP_PIPE_83904", 0600) != 0){
        perror("fifo\n");
        printf("det: %s\n", strerror(errno));
        printf("ERROR\ncannot create the response pipe %s\n", strerror(errno));
        return 1;
    }

    //open for reading
    req_pipe = open("REQ_PIPE_83904", O_RDONLY);
    if(req_pipe == -1){
        printf("ERROR\ncannot open the request pipe\n");
        unlink("RESP_PIPE_83904");
        return 1;
    }

    //open for writing
    resp_pipe = open("RESP_PIPE_83904", O_WRONLY);
    if(resp_pipe == -1){
        printf("ERROR\ncannot open the response pipe\n");
        unlink("RESP_PIPE_83904");
        close(req_pipe);
        return 1;
    }

    // Message to send
    send_string(resp_pipe, "BEGIN");
    printf("SUCCESS\n");


    while(1){
        //Size of message request
        unsigned int size = 0;
        //Read the size of field as 1 byte
        read(req_pipe, &size, sizeof(char));

        char request[size];

        if(size == 4){//EXIT
            read(req_pipe, request, size);
            close(resp_pipe);
            close(req_pipe);
            unlink("RESP_PIPE_83904");
            close_shm(shmFd, sharedMem, shm_size);

            free(path);
            close(fd);
            munmap(file_data, file_size);
            break;
        }
        else if(size == 7) { //VARIANT
            read(req_pipe, request, size); 
            send_string(resp_pipe, "VARIANT");
        
            unsigned int value = 83904;
            write(resp_pipe, &value, sizeof(value));
    
            send_string(resp_pipe, "VALUE");
        }else if(size == 8){ //MAP FILE IN MEMORY
            read(req_pipe, request, size);
            request[size] = '\0';

            read_string(req_pipe, &path);

            send_string(resp_pipe, "MAP_FILE");

            fd = open((char*)path, O_RDONLY);
            if(fd == -1){
                send_string(resp_pipe, "ERROR");
                continue;
            }

            file_size = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            
            file_data = (char*)mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
            if(file_data == (void*)-1){
                send_string(resp_pipe, "ERROR");
                continue;
            }

            send_string(resp_pipe, "SUCCESS");
        }else if(size == 10){
            read(req_pipe, request, size);
            request[size] = '\0';
            if(strcmp(request, "CREATE_SHM") == 0){ //CREATE SHARED MEMORY
                shm_size = read_number(req_pipe);
                int success = create_shm(shm_size, &shmFd, &sharedMem);
                send_string(resp_pipe, "CREATE_SHM");
                if(success){
                    send_string(resp_pipe, "SUCCESS");
                }else{
                    send_string(resp_pipe, "ERROR");
                }
            }
        }
        else if(size == 12){
            read(req_pipe, request, size);
            request[size] = '\0';
            if(strcmp(request, "WRITE_TO_SHM") == 0){ //WRITE TO SHARED MEMORY
                unsigned int offset = read_number(req_pipe);
                unsigned int value = read_number(req_pipe);

                char* pvalue = (char*)&value;

                send_string(resp_pipe, "WRITE_TO_SHM");
                if(offset < 0 || offset > shm_size || offset > shm_size - 4){
                    send_string(resp_pipe, "ERROR");
                    continue;
                }

                for(int i = 0; i < 4; i++){
                    sharedMem[i + offset] = pvalue[i];
                }  

                send_string(resp_pipe, "SUCCESS");
            }
        }
        else if(size == 21){ //READ FROM FILE OFFSET
            read(req_pipe, request, size);
            request[size] = '\0';
            size_t offset = read_number(req_pipe);
            size_t no_bytest = read_number(req_pipe);

            send_string(resp_pipe, "READ_FROM_FILE_OFFSET");
            if(offset + no_bytest > (size_t)file_size){
                send_string(resp_pipe, "ERROR");
                continue;
            }

            memcpy((char*)sharedMem, file_data + offset, no_bytest);

            send_string(resp_pipe, "SUCCESS");
        }
        else if(size == 22){ // READ FROM FILE SECTION
            read(req_pipe, request, size);
            request[size] = '\0';

            unsigned int section_no = read_number(req_pipe);
            size_t offset = read_number(req_pipe);
            size_t no_bytes = read_number(req_pipe);

            send_string(resp_pipe, "READ_FROM_FILE_SECTION");

            sections = file_data[7];  // number of sections from file

            if(section_no > sections || section_no == 0){
                send_string(resp_pipe, "ERROR");
                continue;
            }
            
            unsigned int section_offset = 0;
            unsigned int section_size = 0;

            calculate_section_size_offset(section_no, &section_size, &section_offset, file_data);

            memcpy((char*)sharedMem, file_data + section_offset + offset, no_bytes);

            send_string(resp_pipe, "SUCCESS");

        }
        else if(size == 30){ //READ FROM LOGICAL SPACE OFFSET
            read(req_pipe, request, size);
            request[size] = '\0';

            sections = file_data[7];  // number of sections from file

            unsigned int logical_offset = read_number(req_pipe);
            unsigned int no_bytes = read_number(req_pipe);

            unsigned int section_sizes[sections], section_offsets[sections];

            for(int i=0; i<sections; i++){
                unsigned int section_offset = 0;
                unsigned int section_size = 0;
                calculate_section_size_offset(i+1, &section_size, &section_offset, file_data);
                section_sizes[i] = section_size;
                section_offsets[i] = section_offset;
            }

            unsigned int section_start_logical_offsets[sections];

            calculate_start_logical_offsets(sections, section_start_logical_offsets, section_sizes);
            int found = find_requested_logical_offset(section_start_logical_offsets, section_sizes, logical_offset, sharedMem, no_bytes, section_offsets, file_data, sections);

            send_string(resp_pipe, "READ_FROM_LOGICAL_SPACE_OFFSET");

            if(!found){
                send_string(resp_pipe, "ERROR");
            }else{
                send_string(resp_pipe, "SUCCESS");
            }
        }
        else {
            close(resp_pipe);
            close(req_pipe);
            unlink("RESP_PIPE_83904");
            break;
        }
    }

    return 0;
}