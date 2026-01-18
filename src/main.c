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
#include "parser.h"
#include "launcher.h"
#include "rr.h"
#include "util.h"

int main(int argc, char *argv[]) {
    char* scheduler_policy = "none";
    int quantum = 5;
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

    printf("Quantum: %d\n", quantum);
    printf("Scheduler policy: %s\n", scheduler_policy);

    FILE *workload_file = fopen(workload_filename, "r");
    if (workload_file == NULL) {
        printf("Unable to open file \"%s\"\n", workload_filename);
        exit(1);
    }

    fseek(workload_file, 0L, SEEK_END);
    long file_size = ftell(workload_file);
    rewind(workload_file);

    size_t cmds_array_size = 8;
    char **cmds_array = malloc(sizeof(char*) * cmds_array_size);
    
    size_t cmds_count = 0;
    char cmd_str_buffer[file_size + 1];
    
    while (fgets(cmd_str_buffer, (int) file_size, workload_file) != NULL) {

        size_t cmd_str_len = strlen(cmd_str_buffer);
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

    size_t num_jobs = 0;
    Job *jobs = parse_lines(cmds_array, cmds_count, &num_jobs);

    int active_jobs = launch_jobs(jobs, num_jobs);

    printf("launch jobs result: %d\n", active_jobs);

    int status = round_robin(jobs, num_jobs, quantum);

    printf("round robin result: %d\n", status);

    print_jobs_debug_temp(jobs, num_jobs);

    return 0;
}