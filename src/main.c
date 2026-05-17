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
#include <getopt.h>
#include "job.h"
#include "parser.h"
#include "launcher.h"
#include "rr.h"
#include "util.h"
#include "cli_parser.h"

int main(int argc, char *argv[]) {
    Config config;

    if (!parse_args(argc, argv, &config)) {
        print_usage(argv[0]);
        return 1;
    }

    printf("Quantum: %d\n", config.quantum_ms);
    printf("Scheduler policy: %s\n", config.policy);

    FILE *workload_file = fopen(config.workload_path, "r");
    if (workload_file == NULL) {
        perror(config.workload_path);
        return 1;
    }

    size_t cmds_array_size = 8;
    char **cmds_array = malloc(sizeof(char*) * cmds_array_size);
    if (cmds_array == NULL) {
        perror("malloc");
        fclose(workload_file);
        return 1;
    }
    
    size_t cmds_count = 0;
    char *line = NULL;
    size_t line_capacity = 0;
    ssize_t line_len;
    
    while ((line_len = getline(&line, &line_capacity, workload_file)) != -1) {
        (void)line_len;
        if (cmds_count == cmds_array_size) {
            cmds_array_size *= 2;
            char **resized_cmds_array = realloc(cmds_array, sizeof(char*) * cmds_array_size);
            if (resized_cmds_array == NULL) {
                perror("realloc");
                free(line);
                for (size_t i = 0; i < cmds_count; i++) {
                    free(cmds_array[i]);
                }
                free(cmds_array);
                fclose(workload_file);
                return 1;
            }
            cmds_array = resized_cmds_array;
        }

        cmds_array[cmds_count] = line;
        cmds_count++;
        line = NULL;
        line_capacity = 0;
    }

    free(line);

    if (ferror(workload_file)) {
        perror(config.workload_path);
        for (size_t i = 0; i < cmds_count; i++) {
            free(cmds_array[i]);
        }
        free(cmds_array);
        fclose(workload_file);
        return 1;
    }

    fclose(workload_file);

    size_t num_jobs = 0;
    Job *jobs = parse_lines(cmds_array, cmds_count, &num_jobs);

    for (size_t i = 0; i < cmds_count; i++) {
        free(cmds_array[i]);
    }

    free(cmds_array);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);

    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
        return -1;
    }

    pid_t children_pgid;
    int active_jobs = launch_jobs(jobs, num_jobs, &children_pgid);

    print_jobs_debug_temp(jobs, num_jobs);

    for (size_t i = 0; i < num_jobs; i++) {
        printf("PID: %d, PGID: %d\n", jobs[i].pid, getpgid(jobs[i].pid));
    }

    printf("launch jobs result: %d\n", active_jobs);

    printf("children pgid: %d\n", children_pgid);

    int status = round_robin(jobs, num_jobs, config.quantum_ms, children_pgid);

    printf("round robin result: %d\n", status);

    print_jobs_debug_temp(jobs, num_jobs);

    free_job_array(jobs, num_jobs);

    return 0;
}
