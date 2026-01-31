#include "job.h"
#include <stdlib.h>


Job make_empty_job(void) {
    Job job;
    job.argv = NULL;
    job.argc = 0;
    job.pid = -1;
    job.status = EMPTY;
    job.exit_code = -1;
    job.terminating_signal = -1;
    return job;
}

void free_job_contents(Job *job) {
    for (size_t i = 0; i < job->argc; i++) {
        free(job->argv[i]);
    }
    free(job->argv);
}

void free_job_array(Job *jobs, size_t num_jobs) {
    for (size_t i = 0; i < num_jobs; i++) {
        free_job_contents(&jobs[i]);
    }
    free(jobs);
}

JobStatus interpret_status(int st) {
    if (WIFEXITED(st)) {
        return EXITED;
    } else if (WIFSIGNALED(st)) {
        return TERMINATED;
    } else if (WIFSTOPPED(st)) {
        return STOPPED;
    } else if (WIFCONTINUED(st)) {
        return RUNNING;
    } else {
        return LAUNCH_FAILURE;
    }
}