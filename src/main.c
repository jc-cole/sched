#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdbool.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/timerfd.h>
#include <sys/signalfd.h>
#include <poll.h>
#include <time.h>
#include <unistd.h>
#include "job.h"

#define MAX_PROCESSES 256
#define MAX_LINE_SIZE 256
#define MAX_COMMAND_LINE_ARGS 63

typedef enum ProcessState { RUNNABLE, BLOCKED, DONE, EXECFAILURE } ProcessState;

typedef struct ProcessNode {
    char *path_to_executable;
    char **argv;
    int argc;
    pid_t pid;
    struct ProcessNode *next;
    struct ProcessNode *prev;
    ProcessState state;
    int exit_code;
    int terminating_signal;
} ProcessNode;

void print_jobs_debug(Job *jobs, int num_jobs) {
    printf("Current job statuses:\n");
    for (int i = 0; i < num_jobs; i++) {
        printf("index: %d\n", i);
        printf("    argc: %d\n", jobs[i].argc);
        if (jobs[i].argv != NULL) {
            printf("    argv:\n");
            for (int arg = 0; arg < jobs[i].argc; arg++) {
                printf("        %d. %s\n", arg, jobs[i].argv[arg]);
            }
        } else {
            printf("    argv: NULL\n");
        }
        printf("    pid: %d\n", jobs[i].pid);
        printf("    status: %s\n", job_status_str[jobs[i].status]);
        printf("    exit code: %d\n", jobs[i].exit_code);
        printf("    terminating signal: %d\n", jobs[i].terminating_signal);
        printf("\n");
    }
}


// int launch_tasks(ProcessNode processes[], int cmds_count) {
//     for (int i = 0; i < cmds_count; i++) {
//         int pipe_fd[2];
//         if (pipe(pipe_fd) == -1) {
//             // handle error
//         }
//         fcntl(pipe_fd[1], F_SETFD, FD_CLOEXEC);

//         pid_t pid = fork();
//         if (pid == 0) {
//             close(pipe_fd[0]);
//             raise(SIGSTOP);
//             execvp(processes[i].path_to_executable, processes[i].argv);
//             write(pipe_fd[1], &errno, sizeof(errno));
//             _exit(127);
//         } else {
//             close(pipe_fd[1]);
//             int st;
//             waitpid(pid, &st, WUNTRACED);
//             kill(pid, SIGCONT);
//             int e;
//             ssize_t n = read(pipe_fd[0], &e, sizeof(e));
//             if (n > 0) {
//                 // exec failed
//                 processes[i].state = EXECFAILURE;
//                 processes[i].pid = pid;
//                 continue;
//             }
//             kill(pid, SIGSTOP);
//             int exit_code = waitpid(pid, &st, WUNTRACED);
//             // inspect status to see if process has exited or stopped
//             if (WIFEXITED(st)) {
//                 processes[i].state = DONE;
//                 processes[i].exit_code = exit_code;
//             } else if (WIFSTOPPED(st)) {
//                 processes[i].state = RUNNABLE;
//                 processes[i].exit_code = -1;
//             }
//             processes[i].pid = pid;
//         }
//     }
//     return 0;

int main(int argc, char *argv[]) {
    char* scheduler_policy = NULL;
    int quantum = 500;
    char* workload_filename = NULL;
    if (argc > 0) {
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--policy") == 0) {
                scheduler_policy = argv[++i];
            }
            else if (strcmp(argv[i], "--quantum") == 0) {
                quantum = atoi(argv[++i]);
            } else {
                workload_filename = argv[i];
            }
        }
    } else {
        printf("usage: ./sched --policy <policy> --quantum <ms> <workload.txt>\n");
        exit(1);
    }

    FILE *workload_file = fopen(workload_filename, "r");
    if (workload_file == NULL) {
        printf("Unable to open file \"%s\"\n", workload_filename);
        exit(1);
    }

    fseek(workload_file, 0L, SEEK_END);
    long file_size = ftell(workload_file);
    rewind(workload_file);

    int cmds_array_size = 8;
    char **cmds_array = malloc(sizeof(char*) * cmds_array_size);
    
    int cmds_count = 0;
    char cmd_str_buffer[file_size + 1];
    
    while (fgets(cmd_str_buffer, file_size, workload_file) != NULL) {

        int cmd_str_len = strlen(cmd_str_buffer);
        cmds_array[cmds_count] = malloc(sizeof(char) * (cmd_str_len + 1));

        
        strcpy(cmds_array[cmds_count], cmd_str_buffer);

        for (int i = 0; i < file_size + 1; i++) {
            cmd_str_buffer[i] = 0;
        }

        cmds_count++;

        if (cmds_count == cmds_array_size) {
            cmds_array_size *= 2;
            cmds_array = realloc(cmds_array, sizeof(char*) * cmds_array_size);
        }

    }

    fclose(workload_file);

    for (int i = 0; i < cmds_count; i++) {
        printf("printing lines: \n");
        printf("%s\n", cmds_array[i]);
    }

    int num_jobs = 0;
    Job *jobs = parse_lines(cmds_array, cmds_count, &num_jobs);

    print_jobs_debug(jobs, num_jobs);
    return 0;
}