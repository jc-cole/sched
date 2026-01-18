
#include "reap.h"

int reap_and_update(Job *jobs, size_t num_jobs, size_t *active_jobs, int* reported_statuses_debug) {
    pid_t pid;
    int st;
    while (1) {
        pid = waitpid(-1, &st, WNOHANG | WUNTRACED | WCONTINUED);

        if (pid == 0) {
            return 0;
        }

        if (pid == -1) {
            return -1;
        }

        // debug
        if (reported_statuses_debug != NULL)
            reported_statuses_debug[interpret_status(st)]++;

        int search_success = 0;
        for (size_t i = 0; i < num_jobs; i++) {
            
            if (jobs[i].pid == pid && (jobs[i].status == STOPPED || jobs[i].status == RUNNING)) {

                JobStatus new_status = interpret_status(st);

                if (active_jobs != NULL && !(new_status == RUNNING || new_status == STOPPED)) {
                    (*active_jobs)--;
                }

                jobs[i].status = new_status;

                search_success = 1;
                break;
            }
        }

        if (!search_success) {
            return -1;
        }
    }
}