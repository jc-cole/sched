#include "util.h"
#include <stdarg.h>

static bool verbose_output = false;

void set_verbose_output(bool enabled) {
    verbose_output = enabled;
}

void verbose_printf(const char *format, ...) {
    if (!verbose_output) {
        return;
    }

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

static void print_job_command(const Job *job) {
    for (size_t i = 0; i < job->argc; i++) {
        if (i > 0) {
            printf(" ");
        }
        printf("%s", job->argv[i]);
    }
}

void print_job_launch_verbose(const Job *job, pid_t pgid) {
    if (!verbose_output) {
        return;
    }

    printf("launch: pid=%ld pgid=%ld command=\"", (long)job->pid, (long)pgid);
    print_job_command(job);
    printf("\"\n");
}

static void format_job_status(const Job *job, char *buffer, size_t buffer_size) {
    switch (job->status) {
        case EXITED:
            snprintf(buffer, buffer_size, "exited(%d)", job->exit_code);
            break;
        case TERMINATED:
            snprintf(buffer, buffer_size, "signal(%d)", job->terminating_signal);
            break;
        default:
            snprintf(buffer, buffer_size, "%s", job_status_str[job->status]);
            break;
    }
}

void print_jobs_summary(Job *jobs, size_t num_jobs) {
    printf("PID      STATUS        QUANTA   COMMAND\n");
    for (size_t i = 0; i < num_jobs; i++) {
        char status_buffer[32];
        format_job_status(&jobs[i], status_buffer, sizeof(status_buffer));

        printf("%-8ld %-13s %-8zu ", (long)jobs[i].pid, status_buffer, jobs[i].quanta_scheduled);
        print_job_command(&jobs[i]);
        printf("\n");
    }
}

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
