#ifndef JOB_H
#define JOB_H

#include <fcntl.h>
#include <stdlib.h>

typedef enum JobStatus {
    RUNNING,
    STOPPED,
    EXITED,
    TERMINATED,
    EXEC_FAILURE,
    LAUNCH_FAILURE,
    EXEC_READY,
    EMPTY,
    SYNTAX_ERROR,
    STATE_COUNT
} JobStatus;

static char *const job_status_str[STATE_COUNT] = {
    [RUNNING]        = "RUNNING",
    [STOPPED]        = "STOPPED",
    [EXITED]         = "EXITED",
    [TERMINATED]     = "TERMINATED",
    [EXEC_FAILURE]   = "EXEC_FAILURE",
    [LAUNCH_FAILURE] = "LAUNCH_FAILURE",
    [EXEC_READY]     = "EXEC_READY",
    [EMPTY]          = "EMPTY",
    [SYNTAX_ERROR]   = "SYNTAX_ERROR",
};

typedef struct Job {
    char **argv;
    size_t argc;
    pid_t pid;
    JobStatus status;
    int exit_code;
    int terminating_signal;
} Job;

Job make_empty_job(void);

void free_job_contents(Job job);

#endif