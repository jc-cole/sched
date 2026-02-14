#include "rr.h"
#include "reap.h"

// debug
int reported_statuses[STATE_COUNT];

int round_robin(Job *jobs, size_t num_jobs, int quantum_ms, pid_t children_pid) {

    // debug
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

    int sigint_fd = make_sigint_fd();
    if (sigint_fd == -1) {
        return -1;
    }

    struct pollfd poll_fds[3];

    poll_fds[0].fd = timer_fd;
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = sigchld_fd;
    poll_fds[1].events = POLLIN;

    poll_fds[2].fd = sigint_fd;
    poll_fds[2].events = POLLIN;

    size_t active_jobs = 0;
    for (size_t i = 0; i < num_jobs; i++) {
        if (jobs[i].status == STOPPED) {
            active_jobs++;
        }
    }
    
    for (size_t i = 0; active_jobs > 0; i = (i + 1) % num_jobs) {

        if (jobs[i].status != STOPPED) {
            continue;
        }

        if (kill(jobs[i].pid, SIGCONT) == -1) {
            perror("kill");
            exit(EXIT_FAILURE);
        }

        if (jobs[i].quanta_scheduled < SIZE_MAX) {
            jobs[i].quanta_scheduled++;
        }
        

        int st;
        if (waitpid(jobs[i].pid, &st, WUNTRACED | WCONTINUED) == -1) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }
        
        JobStatus status = update_job_status(&jobs[i], st, &active_jobs);
        if (status != RUNNING) {
            continue;
        }

        if (reset_timer(timer_fd, quantum_ms) == -1) {
            // error message from reset_timer
            return -1;
        }

        while (1) {

            if (poll(poll_fds, 3, -1) == -1) {
                perror("poll");
                exit(EXIT_FAILURE);
            }

            if (poll_fds[1].revents & POLL_IN) { // sigchld

                struct signalfd_siginfo si;
                ssize_t n = read(sigchld_fd, &si, sizeof(si));
                if (n == -1) {
                    perror("read");
                    exit(EXIT_FAILURE);
                }
                if (n != sizeof(si)) {
                    // error message
                    return -1;
                }

                if (reap_and_update(jobs, num_jobs, &active_jobs, reported_statuses) == -1) {
                    // error message handle by reap_and_update
                    return -1;
                }

                if (jobs[i].status != RUNNING) {
                    break;
                }
            }

            if (poll_fds[2].revents & POLL_IN) { // sigint

                struct signalfd_siginfo si;
                ssize_t n = read(sigint_fd, &si, sizeof(si));
                if (n == -1) {
                    perror("read");
                    exit(EXIT_FAILURE);
                }
                if (n != sizeof(si)) {
                    // error message
                    return -1;
                }
                /////
                return sigint_protocol(jobs, num_jobs, poll_fds, children_pid, &active_jobs);
            }

            if (poll_fds[0].revents & POLL_IN) { // quantum expiry

                uint64_t exp_count;
                ssize_t bytes_read = read(timer_fd, &exp_count, sizeof(uint64_t));

                if (bytes_read == -1) {
                    perror("kill");
                    exit(EXIT_FAILURE);
                }

                if (bytes_read != sizeof(uint64_t)) {
                    // error message
                    return -1;
                }

                if (kill(jobs[i].pid, SIGSTOP) == -1) {
                    perror("kill");
                    exit(EXIT_FAILURE);
                }

                if (waitpid(jobs[i].pid, &st, WUNTRACED | WCONTINUED) == -1) {
                    perror("waitpid");
                    exit(EXIT_FAILURE);
                }

                update_job_status(&jobs[i], st, &active_jobs);

                break;
            }
        }
    }

    return 0;
}

