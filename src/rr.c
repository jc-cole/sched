#include "rr.h"

// debug
int reported_statuses[STATE_COUNT];

static int reap_and_update(Job *jobs, size_t num_jobs, size_t *active_jobs) {
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
        reported_statuses[interpret_status(st)]++;

        int search_success = 0;
        for (size_t i = 0; i < num_jobs; i++) {
            
            if (jobs[i].pid == pid && (jobs[i].status == STOPPED || jobs[i].status == RUNNING)) {

                JobStatus new_status = interpret_status(st);

                if (!(new_status == RUNNING || new_status == STOPPED)) {
                    (*active_jobs)--;
                }

                jobs[i].status = new_status;

                search_success = 1;
                break;
            }
        }

        if (!search_success) {
            printf("here\n");
            return -1;
        }
    }
}

int round_robin(Job *jobs, size_t num_jobs, int quantum_ms) {

    // debug stuff
    for (int i = 0; i < STATE_COUNT; i++) {
        reported_statuses[i] = 0;
    }

    int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timer_fd == -1) {
        return -1;
    }

    int sigchld_fd = make_sigchld_fd();
    if (sigchld_fd == -1) {
        return -1;
    }

    struct pollfd poll_fds[2];

    poll_fds[0].fd = timer_fd;
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = sigchld_fd;
    poll_fds[1].events = POLLIN;

    size_t active_jobs = 0;
    for (size_t i = 0; i < num_jobs; i++) {
        if (jobs[i].status == STOPPED) {
            active_jobs++;
        }
    }

    for (size_t i = 0; active_jobs > 0; i = (i + 1) % num_jobs) {

        if (jobs[i].status != STOPPED) {
            if (reap_and_update(jobs, num_jobs, &active_jobs) == -1) {
                return -1;
            }
            continue;
        }

        if (kill(jobs[i].pid, SIGCONT) == -1) {
            return -1;
        }

        int st;
        if (waitpid(jobs[i].pid, &st, WUNTRACED | WCONTINUED) == -1) {
            return -1;
        }
        
        JobStatus status = interpret_status(st);
        if (status != RUNNING) {
            jobs[i].status = status;
            continue;
        }

        if (reset_timer(timer_fd, quantum_ms) == -1) {
            return -1;
        }

        while (1) {

            if (poll(poll_fds, 2, -1) == -1) {
                return -1;
            }

            if (poll_fds[1].revents & POLL_IN) { // sigchld

                struct signalfd_siginfo si;
                ssize_t n = read(sigchld_fd, &si, sizeof(si));
                if (n != sizeof(si)) {
                    return -1;
                }

                if (reap_and_update(jobs, num_jobs, &active_jobs) == -1) {
                    return -1;
                }

                if (jobs[i].status != RUNNING) {
                    break;
                }
            }

            if (poll_fds[0].revents & POLL_IN) { // quantum expiry

                uint64_t exp_count;
                ssize_t bytes_read = read(timer_fd, &exp_count, sizeof(uint64_t));
                if (bytes_read == sizeof(uint64_t)) {
                    return -1;
                }

                if (kill(jobs[i].pid, SIGSTOP) == -1) {
                    return -1;
                }

                if (waitpid(jobs[i].pid, &st, WUNTRACED | WCONTINUED) == -1) {
                    return -1;
                }

                jobs[i].status = interpret_status(st);
                break;
            }
        }
    }

    return 0;
}

