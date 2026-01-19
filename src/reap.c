
#include "reap.h"

// decrements active_job_count if passed whenever the given job was previously active (stopped/running) 
// and became inactive (not stopped/running)
JobStatus update_job_status(Job *job, int wpid_status, size_t *active_job_count) {

    JobStatus new_status = interpret_status(wpid_status);

    if (
        (job->status == RUNNING || job->status == STOPPED)
        && !(new_status == RUNNING || new_status == STOPPED)
        && (active_job_count != NULL)
    ) {
        (*active_job_count)--;
    }

    job->status = new_status;

    if (new_status == EXITED) {
        job->exit_code = WEXITSTATUS(wpid_status);
    } else if (new_status == TERMINATED) {
        job->terminating_signal = WTERMSIG(wpid_status);
    }

    return new_status;
}


int reap_and_update(Job *jobs, size_t num_jobs, size_t *active_job_count, int* reported_statuses_debug) {

    while (1) {
        pid_t pid;
        int st;
        pid = waitpid(-1, &st, WNOHANG | WUNTRACED | WCONTINUED);

        if (pid == 0) {
            return 0;
        }

        if (pid == -1) {
            return -1;
        }

        // debug
        if (reported_statuses_debug != NULL) {
            reported_statuses_debug[interpret_status(st)]++;
        }

        for (size_t i = 0; i < num_jobs; i++) {
            if (jobs[i].pid == pid) {
                update_job_status(&jobs[i], st, active_job_count);
                return 0;
            }
        }

        return -1; // job with reaped pid not found in job state array
    }
}