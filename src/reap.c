
#include "reap.h"
#include "timer.h"
#include "util.h"

// decrements active_job_count if passed whenever the given job was previously active (stopped/running) 
// and became inactive (not stopped/running)
JobStatus update_job_status(Job *job, int wpid_status, size_t *active_job_count) {


    JobStatus old_status = job->status;
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

    if (new_status != old_status) {
        switch (new_status) {
            case RUNNING:
                printf("PID %d: continued\n", job->pid);
                break;
            case STOPPED:
                printf("PID %d: stopped\n", job->pid);
                break;
            case EXITED:
                printf("PID %d: exited with code %d\n", job->pid, job->exit_code);
                break;
            case TERMINATED:
                printf("PID %d: terminated by signal %d\n", job->pid, job->terminating_signal);
                break;
            default:
                break;
        }
    }

    return new_status;
}

int sigint_protocol(Job *jobs, size_t num_jobs, struct pollfd poll_fds[3], pid_t children_pid, size_t *active_job_count) {

    int SIGTERM_GRACE_PERIOD_MS = 1000;

    reset_timer(poll_fds[3].fd, SIGTERM_GRACE_PERIOD_MS);

    if (kill(-children_pid, SIGCONT) == -1) {
        perror("kill");
        exit(EXIT_FAILURE);
    }

    if (kill(-children_pid, SIGTERM) == -1) {
        perror("kill");
        exit(EXIT_FAILURE);
    }

    while (*active_job_count > 0) {

        print_jobs_debug_temp(jobs, num_jobs);

        if (poll(poll_fds, 3, -1) == -1) {
            perror("poll");
            exit(EXIT_FAILURE);
        }

        if (poll_fds[1].revents & POLL_IN) { // sigchld

            struct signalfd_siginfo si;
            ssize_t n = read(poll_fds[1].fd, &si, sizeof(si));
            if (n == -1) {
                perror("read");
                exit(EXIT_FAILURE);
            }
            if (n != sizeof(si)) {
                // error message
                return -1;
            }

            if (reap_and_update(jobs, num_jobs, active_job_count, NULL) == -1) {
                // error message
                return -1;
            }

            continue;
        }

        if (poll_fds[2].revents & POLL_IN) { // sigint (double ctrl c)

            struct signalfd_siginfo si;
            ssize_t n = read(poll_fds[2].fd, &si, sizeof(si));
            if (n == -1) {
                perror("read");
                exit(EXIT_FAILURE);
            }
            if (n != sizeof(si)) {
                // error message
                return -1;
            }

            if (kill(-children_pid, SIGKILL) == -1) {
                perror("kill");
                exit(EXIT_FAILURE);
            }

            continue;
            
        }

        if (poll_fds[0].revents & POLL_IN) { // grace period expiry

            uint64_t exp_count;
            ssize_t bytes_read = read(poll_fds[0].fd, &exp_count, sizeof(uint64_t));

            if (bytes_read == -1) {
                perror("kill");
                exit(EXIT_FAILURE);
            }

            if (bytes_read != sizeof(uint64_t)) {
                // error message
                return -1;
            }

            if (kill(-children_pid, SIGKILL) == -1) {
                perror("kill");
                exit(EXIT_FAILURE);
            }

            continue;
        }
    }

    return 0;
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

        bool search_success = false;
        for (size_t i = 0; i < num_jobs; i++) {
            if (jobs[i].pid == pid) {
                update_job_status(&jobs[i], st, active_job_count);
                search_success = true;
                break;
            }
        }

        if (!search_success) {
            return -1;
        } // job with reaped pid not found in job state array
    }

}