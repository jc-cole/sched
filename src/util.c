#include "util.h"

void print_jobs_debug_temp(Job *jobs, size_t num_jobs) {
    printf("Current job statuses:\n");
    for (size_t i = 0; i < num_jobs; i++) {
        printf("index: %zu\n", i);
        printf("    argc: %zu\n", jobs[i].argc);
        if (jobs[i].argv != NULL) {
            printf("    argv:\n");
            for (size_t arg = 0; arg < jobs[i].argc; arg++) {
                printf("        %zu. %s\n", arg, jobs[i].argv[arg]);
            }
        } else {
            printf("    argv: NULL\n");
        }
        printf("    pid: %ld\n", (long) jobs[i].pid);
        printf("    status: %s\n", job_status_str[jobs[i].status]);
        printf("    exit code: %d\n", jobs[i].exit_code);
        printf("    terminating signal: %d\n", jobs[i].terminating_signal);
        printf("    total scheduled quanta: %zu\n", jobs[i].quanta_scheduled);

        printf("\n");
    }
}

void print_jobs_by_status(Job *jobs, size_t num_jobs, JobStatus matching_status) {
    printf("Matching statuses:\n");
    size_t matching_count = 0;
    for (size_t i = 0; i < num_jobs; i++) {
        if (jobs[i].status == matching_status) {
            matching_count++;
            printf("index: %zu\n", i);
            printf("    argc: %zu\n", jobs[i].argc);
            if (jobs[i].argv != NULL) {
                printf("    argv:\n");
                for (size_t arg = 0; arg < jobs[i].argc; arg++) {
                    printf("        %zu. %s\n", arg, jobs[i].argv[arg]);
                }
            } else {
                printf("    argv: NULL\n");
            }
            printf("    pid: %ld\n", (long) jobs[i].pid);
            printf("    status: %s\n", job_status_str[jobs[i].status]);
            printf("    exit code: %d\n", jobs[i].exit_code);
            printf("    terminating signal: %d\n", jobs[i].terminating_signal);
            printf("\n");
        }
    }
    printf("Count matching requested status: %zu\n", matching_count);
}