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
    struct launcProcessNode *prev;
    ProcessState state;
    int exit_code;
} ProcessNode;


void free_parse_result(ProcessNode parse_result) {
    free(parse_result.argv);
}

void log_processes(ProcessNode processes[], int cmds_count) {
    for (int i = 0; i < cmds_count; i++) {
        printf("PID: %ld\n", (long) processes[i].pid);
        printf("State: %d\n", processes[i].state);
        printf("Executable path: %s\n", processes[i].path_to_executable);
        printf("argc: %d\n", processes[i].argc);
        printf("\n");
    }
}

int launch_tasks(ProcessNode processes[], int cmds_count) {
    for (int i = 0; i < cmds_count; i++) {
        int pipe_fd[2];
        if (pipe(pipe_fd) == -1) {
            // handle error
        }
        fcntl(pipe_fd[1], F_SETFD, FD_CLOEXEC);

        pid_t pid = fork();
        if (pid == 0) {
            close(pipe_fd[0]);
            raise(SIGSTOP);
            execvp(processes[i].path_to_executable, processes[i].argv);
            write(pipe_fd[1], &errno, sizeof(errno));
            _exit(127);
        } else {
            close(pipe_fd[1]);
            int st;
            waitpid(pid, &st, WUNTRACED);
            kill(pid, SIGCONT);
            int e;
            ssize_t n = read(pipe_fd[0], &e, sizeof(e));
            if (n > 0) {
                // exec failed
                processes[i].state = EXECFAILURE;
                processes[i].pid = pid;
                continue;
            }
            kill(pid, SIGSTOP);
            int exit_code = waitpid(pid, &st, WUNTRACED);
            // inspect status to see if process has exited or stopped
            if (WIFEXITED(st)) {
                processes[i].state = DONE;
                processes[i].exit_code = exit_code;
            } else if (WIFSTOPPED(st)) {
                processes[i].state = RUNNABLE;
                processes[i].exit_code = -1;
            }
            processes[i].pid = pid;
        }
    }
    return 0;
}



void connect_process_nodes(ProcessNode processes_array[], int cmds_count) {
    for (int i = 0; i < cmds_count - 1; i++) {
        processes_array[i].next = &processes_array[i+1];
    }
    processes_array[cmds_count].next = &processes_array[0];

    for (int i = cmds_count - 1; i > 0; i--) {
        processes_array[i].prev = &processes_array[i-1];
    }
    processes_array[0].prev = &processes_array[cmds_count-1];
}

// Policy: Splits line on any stretch of tabs/spaces (not newlines)
// Note: Mutates passed string
ProcessNode parse_line(char *line) {

    ProcessNode result;
    result.path_to_executable = NULL;
    result.argc = 1;
    result.argv = (char *) malloc(sizeof(char *) * (MAX_COMMAND_LINE_ARGS + 1));
    for (int i = 0; i < MAX_COMMAND_LINE_ARGS + 1; i++) {
        result.argv[i] = NULL;
    }

    int line_length = strlen(line);

    bool on_leading_spaces = true;

    // first extract path to executable
    int i = 0;
    for (; i < line_length; i++) {
        if ((line[i] == ' ' || line[i] == '\t') && on_leading_spaces) {
            continue;
        } else if ((line[i] == ' ' || line[i] == '\t') && !on_leading_spaces) {
            line[i] = '\0'; // reached end of path to executable but still more args
            i++;
            result.argv[0] = result.path_to_executable;
            break;
        } else if (line[i] == '\0') {
            result.argv[0] = result.path_to_executable;
            result.argv[1] = NULL;
            return result;
        } else if (result.path_to_executable == NULL) {
            on_leading_spaces = false;
            result.path_to_executable = &line[i];
        }
    }

    // now iteratively extract argument strings
    on_leading_spaces = true;
    for (; i < line_length; i++) {
        if ((line[i] == ' ' || line[i] == '\t') && on_leading_spaces) {
            continue;
        } else if ((line[i] == ' ' || line[i] == '\t') && !on_leading_spaces) {
            line[i] = '\0'; // reached end of path to executable but still more args
        } else if (line[i] == '\0') {
            return result;
        } else if (result.argv[result.argc] == NULL) {
            on_leading_spaces = false;
            result.argv[result.argc] = &line[i];
            result.argc++;
        }
    }

    return result;
}


int main(int argc, char *argv[]) {
    char* scheduler_policy = NULL;
    int quantum = 100;
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

    char cmds_array[MAX_PROCESSES][MAX_LINE_SIZE];
    int cmds_count = 0;

    
    while (fgets(cmds_array[cmds_count], MAX_LINE_SIZE, workload_file)) {
        int command_str_len = strlen(cmds_array[cmds_count]);
        if (cmds_array[cmds_count][command_str_len - 1] == '\n') {
            cmds_array[cmds_count][command_str_len - 1] = '\0';
        }
        cmds_count++;
    }

    ProcessNode processes[cmds_count];
    for (int i = 0; i < cmds_count; i++) {
        processes[i] = parse_line(cmds_array[i]);
    }

    connect_process_nodes(processes, cmds_count);

    launch_tasks(processes, cmds_count);

    log_processes(processes, cmds_count);
    
    return 0;
}