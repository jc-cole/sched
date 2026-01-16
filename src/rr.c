#include "rr.h"

static int handle_sigchld(Job *jobs, size_t num_jobs, size_t *active_jobs) {
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
        
        for (size_t i = 0; i < num_jobs; i++) {
            if (jobs[i].pid == pid) {

                JobStatus new_status = interpret_status(st);

                if (!(new_status == RUNNING || new_status == STOPPED)) {
                    active_jobs--;
                }

                jobs[i].status = new_status;
                break;
            }
        }
    }
}

int round_robin(Job *jobs, size_t num_jobs, int quantum_ms) {

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

    size_t active_jobs = num_jobs;
    for (size_t i = 0; active_jobs > 0; i = ++i % num_jobs) {
        if (jobs[i].status != STOPPED) {
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
            int n = poll(poll_fds, 2, -1);

            if (poll_fds[1].revents & POLL_IN) { // sigchld

                struct signalfd_siginfo si;
                ssize_t n = read(sigchld_fd, &si, sizeof(si));
                if (n != sizeof(si)) {
                    return -1;
                }

                if (handle_sigchld(jobs, num_jobs, &active_jobs) == -1) {
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

                int st;
                if (waitpid(jobs[i].pid, &st, WUNTRACED | WCONTINUED) == -1) {
                    return -1;
                }

                jobs[i].status = interpret_status(st);
                continue;
            }
        }
    }

    return 0;
}

