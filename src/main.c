#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>        
#include <sys/wait.h>
#include "circularBuffer.h"
#include "splitCommand.h"

#define BUF_SIZE 1024

void read_line_into_buffer(CircularBuffer *cb, char *dest, int *reachedEOF) {

    int line_len;
    unsigned char temp_buf[BUF_SIZE];
    while((line_len = buffer_size_next_element(cb, '\n', *reachedEOF)) == -1) {

        int bytes_read = read(0, temp_buf, BUF_SIZE);

        if (bytes_read <= 0){
            *reachedEOF = 1;
            line_len = buffer_size_next_element(cb, '\n', 1);
            if (line_len <= 0) return;
            break;
        }

        for(int i = 0; i < bytes_read; i++) {
            buffer_push(cb, temp_buf[i]);
        }
    }

    for(int i = 0; i < line_len; i++) {
        dest[i] = (char)buffer_pop(cb);
    }
    dest[line_len] = '\0';

    if (line_len > 0 && (dest[line_len - 1] == '\n')) {
        dest[line_len - 1] = '\0';
    }

}

int main(int argc, char *argv[]){

    CircularBuffer cb;
    char line[1024];
    int reachedEOF = 0;

    if (buffer_init(&cb, BUF_SIZE) != 0) return 1;

    while (1) {

        read_line_into_buffer(&cb, line, &reachedEOF);

        if (line[0] == '\0' && reachedEOF) break;

        if (strcmp(line, "EXIT") == 0) break;

        if (strcmp(line, "SINGLE") == 0 || strcmp(line, "CONCURRENT") == 0){
            
            char current_mode[20];
            strcpy(current_mode, line);

            read_line_into_buffer(&cb, line, &reachedEOF);

            char **args = split_command(line);

            pid_t pid = fork();

            if (pid == 0){
                execvp(args[0], args);
                _exit(1);
            }
            else {
                if (strcmp(current_mode, "SINGLE") == 0){
                    waitpid(pid, NULL, 0);
                }
                free(args);
            }

        }
        else if(strcmp(line, "PIPED") == 0) {
            char line1[1024], line2[1024];
            read_line_into_buffer(&cb, line1, &reachedEOF); 
            read_line_into_buffer(&cb, line2, &reachedEOF); 

            char **args1 = split_command(line1);
            char **args2 = split_command(line2);

            int pipefd[2]; 
            pipe(pipefd); 

            if (fork() == 0) { 
                dup2(pipefd[1], 1); 
                close(pipefd[0]); close(pipefd[1]);
                execvp(args1[0], args1);
                _exit(1);
            }
            if (fork() == 0) { 
                dup2(pipefd[0], 0); 
                close(pipefd[0]); close(pipefd[1]);
                execvp(args2[0], args2);
                _exit(1);
            }
            
            close(pipefd[0]); close(pipefd[1]);
            waitpid(-1, NULL, 0); 
            waitpid(-1, NULL, 0);
            
            free(args1); free(args2); 
        }
    }
    buffer_deallocate(&cb); 
    return 0;
}
