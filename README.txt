# Multi-Process Synchronization System
C, POSIX, Linux

## Overview
A multi-process application implementing complex process and thread 
synchronization using POSIX primitives.

## Architecture
- 9 processes organized in a hierarchy using fork()
- Inter-process synchronization via named POSIX semaphores
- Thread synchronization using mutexes and condition variables

## Key Concepts Implemented
- Multi-process hierarchy with parent-child synchronization
- Thread barrier pattern (max N concurrent threads)
- Inter-process thread synchronization across separate processes
- Race condition prevention using mutex + condition variables
- Named semaphores for cross-process communication

## Build
gcc -Wall a2.c a2_helper.c -o a2 -lpthread


# Binary File Parser & IPC System
C, POSIX, Linux

## Overview
A C program that parses a custom binary file format and communicates 
with an external process via a pipe-based binary protocol.

## Key Concepts Implemented
- Binary file format parsing (header, section table, offset calculation)
- Memory-mapped file I/O using mmap()
- POSIX shared memory (shm_open, mmap)
- Named pipe (FIFO) communication with custom binary protocol
- Logical-to-physical address space mapping across file sections

## Build
gcc -Wall a3.c -o a3 -lrt