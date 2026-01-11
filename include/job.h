#include <fcntl.h>

typedef enum JobStatus {
    RUNNING,
    STOPPED,
    EXITED,
    TERMINATED,
    EXEC_FAILURE,
    EXEC_READY,
    EMPTY,
    SYNTAX_ERROR,
    STATE_COUNT
} JobStatus;

static char *const job_status_str[STATE_COUNT] = {
    [RUNNING]       = "RUNNING",
    [STOPPED]       = "STOPPED",
    [EXITED]        = "EXITED",
    [TERMINATED]    = "TERMINATED",
    [EXEC_FAILURE]  = "EXEC_FAILURE",
    [EXEC_READY]    = "EXEC_READY",
    [EMPTY]         = "EMPTY",
    [SYNTAX_ERROR]  = "SYNTAX_ERROR",
};

typedef struct Job {
    char **argv;
    int argc;
    pid_t pid;
    JobStatus status;
    int exit_code;
    int terminating_signal;
} Job;