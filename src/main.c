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


void free_process_node(ProcessNode process_node) {
    free(process_node.argv);
}

void log_processes(ProcessNode processes[], int cmds_count) {
    for (int i = 0; i < cmds_count; i++) {
        printf("PID: %ld\n", (long) processes[i].pid);
        printf("State: %d\n", processes[i].state);
        printf("Executable path: %s\n", processes[i].path_to_executable);
        printf("argc: %d\n", processes[i].argc);
        printf("Points to PID: %d", processes[i].next->pid);
        printf("\n");
    }
}

ProcessNode *get_process_node(pid_t pid, ProcessNode processes[], int cmds_count) {
    for (int i = 0; i < cmds_count; i++) {
        if (processes[i].pid == pid) {
            return &processes[i];
        }
    }
    return NULL;
}

int handle_child_exit(ProcessNode *process_node, int exit_code) {
    process_node->exit_code = exit_code;
    process_node->terminating_signal = -1;
    process_node->state = DONE;
}

int handle_child_term(ProcessNode *process_node, int signal_num) {
    process_node->exit_code = -1;
    process_node->terminating_signal = signal_num;
    process_node->state = DONE;
}

int make_timer_fd(int quantum_ms) {
    int tfd = timerfd_create(CLOCK_MONOTONIC, 0);

    struct itimerspec ts = {0};

    ts.it_value.tv_nsec    = quantum_ms * 1000 * 1000;
    ts.it_interval.tv_nsec = quantum_ms * 1000 * 1000;

    if (timerfd_settime(tfd, 0, &ts, NULL) == -1) {
        perror("timerfd_settime");
    }

    return tfd;
}

int make_sigchld_fd() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);

    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
        perror("sigprocmask");
        return 1;
    }

    int sfd = signalfd(-1, &mask, 0);
    if (sfd == -1) {
        perror("signalfd");
        return -1;
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

int dequeue_node(ProcessNode *node) {
    if (node->next == node) { //if node is pointing to self
        free_process_node(*node);
        return -1;
    }
    node->prev->next = node->next;
    free_process_node(*node);
    return 0;
}

int round_robin(ProcessNode processes[], int cmds_count, int quantum_ms) {
    ProcessNode *current_node = NULL;

    int active_jobs = cmds_count;
    for (int i = 0; i < cmds_count; i++) {
        if (processes[i].state == DONE || processes[i].state == EXECFAILURE) {
            if (dequeue_node(&processes[i]) == -1) {
                return 0;
            }
            active_jobs--;
        } else if (current_node == NULL) {
            current_node = &processes[i];
        }
    }

    struct pollfd pfds[2];

    pfds[0].fd = make_timer_fd(quantum_ms);
    pfds[0].events = POLLIN;

    pfds[1].fd = make_sigchld_fd();
    pfds[1].events = POLLIN;

    while (active_jobs > 0) {
        // next: implement poll + timerfd + signalfd to implement waiting for quantam and signal handling

        kill(current_node->pid, SIGCONT);

        if (poll(pfds, 2, -1) == -1) {
            exit(2);
        }

        if (pfds[0].revents & POLLIN) {
            // quantum expired
            uint64_t expirations;
            ssize_t n = read(pfds[0].fd, &expirations, sizeof(expirations));

            printf("the quantum expired\n");
            kill(current_node->pid, SIGSTOP);
            current_node = current_node->next;
            continue;
        }

        if (pfds[1].revents & POLLIN) {
            // child exited / stopped
            printf("we got a sigchld\n");

            struct signalfd_siginfo si;
            read(pfds[1].fd, &si, sizeof(si));

            pid_t pid;
            int st;
            bool current_process_done = false;
            while ((pid = waitpid(-1, &st, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
                // WIFEXITED / WIFSIGNALED / WIFSTOPPED / WIFCONTINUED
                if (WIFEXITED(st)) {
                    printf("it was an exit. PID %d\n", pid);
                    handle_child_exit(get_process_node(pid, processes, cmds_count), WEXITSTATUS(st));
                    if (pid == current_node->pid) {
                        current_process_done = true;
                    }
                } else if (WIFSIGNALED(st)) {
                    printf("it was a term. PID %d\n", pid);
                    handle_child_term(get_process_node(pid, processes, cmds_count), WTERMSIG(st));
                    if (pid == current_node->pid) {
                        current_process_done = true;
                    }
                } else {
                    printf("it was something else\n");
                }
            }
            if (current_process_done) {
                dequeue_node(&current_node);
                
                continue;
            }

        }
    }

    printf("All jobs complete ");
    log_processes(processes, cmds_count);

}




void connect_process_nodes(ProcessNode processes_array[], int cmds_count) {
    for (int i = 0; i < cmds_count - 2; i++) { // loop up to second to last
        processes_array[i].next = &processes_array[i+1];
    }
    processes_array[cmds_count - 1].next = &processes_array[0]; // manually set last node to point to first

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

    printf("the processes are about to be logged");
    log_processes(processes, cmds_count);
    
    printf("these are the only addys that current_node should take: \n");
    for (int i = 0; i < cmds_count; i++) {
        printf("%p\n", &processes[i]);
    }

    round_robin(processes, cmds_count, quantum);
    
    return 0;
}