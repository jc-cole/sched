#define _GNU_SOURCE

#include "launcher.h"

static int redirect_output_to_log(const char *path) {
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);

    if (fd == -1) {
        perror("open");
        _exit(127);
    }

    if (dup2(fd, STDOUT_FILENO) < 0) {
        perror("dup2");
        _exit(127);
    }
    if (dup2(fd, STDERR_FILENO) < 0) {
        perror("dup2");
        _exit(127);
    }

    // no longer need the original fd if it's not stdout/stderr
    if (fd > STDERR_FILENO) close(fd);
    return 1;
}


// make a pipe from child to parent (pipe2 + O_CLOEXEC)
// fork()
// In child:
//  raise sigstop
//  after getting sigcont, 
//  first write one byte symbol to the pipe
//  execvp
//  if execvp fails, write errno to pipe
//  if execvp succeds, pipe closes automatically, parent gets EOF
// In parent: 
//  waitpid for the sigstop
//  send sigcont
//  read from the pipe
//  if we get an errno, set job status as exec_failure
//  if we get no symbol -> child died before exec, 
//  if we get symbol + EOF, immediately send sigstop to child
//  waitpid child
//  inspect status to see if child stopped -> set status stopped
//  inspect status to see if child exited -> set status exited
//  inspect status to see if child terminated -> set status terminated

static int launch_job(Job *job, pid_t *children_pgid) {
    int pipe[2];
    pipe2(pipe, O_CLOEXEC);
    pid_t pid = fork();
    if (pid == 0) {
        close(pipe[0]);

        if (*children_pgid == -1) { // error handling needed here
            setpgid(0, 0);
        } else {
            setpgid(0, *children_pgid);
        }

        redirect_output_to_log("children.log");
        
        raise(SIGSTOP);

        char marker = 'M';
        if (write(pipe[1], &marker, 1) == -1) {
            _exit(1);
        }

        execvp(job->argv[0], job->argv);

        int err = errno;
        if (write(pipe[1], &err, sizeof(err)) == -1) {
            _exit(1);
        }

        _exit(127);
    } else {
        close(pipe[1]);

        if (*children_pgid == -1) { // error handling needed here
            setpgid(pid, pid);
            *children_pgid = pid;
        } else {
            setpgid(pid, *children_pgid);
        }

        job->pid = pid;

        int st;
        if (waitpid(pid, &st, WUNTRACED) == -1) {
            job->status = LAUNCH_FAILURE;
            printf("Job %s failed to launch. \n", job->argv[0]);
            return -1;
        }

        JobStatus pre_exec_status = interpret_status(st);
        if (pre_exec_status != STOPPED) {
            job->status = LAUNCH_FAILURE;
            printf("Job %s failed to launch. \n", job->argv[0]);
            return -1;
        }

        if (kill(pid, SIGCONT) == -1) {
            job->status = LAUNCH_FAILURE;
            printf("Job %s failed to launch. \n", job->argv[0]);
            return -1;
        }

        char marker;
        ssize_t n = read(pipe[0], &marker, 1);
        if (n <= 0) {
            job->status = LAUNCH_FAILURE;
            printf("Job %s failed to launch. \n", job->argv[0]);
            return -1;
        } else if (marker != 'M') {
            job->status = EXEC_FAILURE;
            printf("Job %s was unable to execvp. \n", job->argv[0]);
            return 1;
        }

        int err = 0;
        size_t got = 0;
        while (got < sizeof(err)) {
            ssize_t r = read(pipe[0], (char*) &err + got, sizeof(err) - got);

            if (r == 0) {
                // got marker, no errno -> success
                break;
            }

            if (r < 0) {
                job->status = LAUNCH_FAILURE;
                printf("Job %s failed to launch. \n", job->argv[0]);
                return -1;
            }

            got += (size_t)r;
        }

        if (got == sizeof(err)) {
            job->status = EXEC_FAILURE;
            printf("Job %s was unable to execvp. \n", job->argv[0]);
            return 1;
        }

        if (kill(pid, SIGSTOP) == -1) {
            job->status = LAUNCH_FAILURE;
            printf("Job %s failed to launch. \n", job->argv[0]);
            return -1;
        }

        if (waitpid(pid, &st, WUNTRACED) == -1) {
            job->status = LAUNCH_FAILURE;
            printf("Job %s failed to launch. \n", job->argv[0]);
            return -1;
        }

        JobStatus post_exec_status = interpret_status(st);
        if (pre_exec_status == LAUNCH_FAILURE) {
            job->status = LAUNCH_FAILURE;
            printf("Job %s failed to launch. \n", job->argv[0]);
            return -1;
        } else {
            printf("Job %s launched with PID %d. \n", job->argv[0], pid);
            job->status = post_exec_status;
            return 0;
        }
    }
}

int launch_jobs(Job *jobs, size_t num_jobs, pid_t *children_pgid) {
    int syscall_error = 0;

    // clear log file
    int fd = open("children.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("open children.log"); exit(1); }
    close(fd);

    pid_t sched_pgid = getpgrp();
    tcsetpgrp(STDIN_FILENO, sched_pgid); 

    *children_pgid = -1;
    for (size_t i = 0; i < num_jobs; i++) {
        if (launch_job(&jobs[i], children_pgid) == -1) {
            syscall_error = 1;
        }
    }

    if (reap_and_update(jobs, num_jobs, NULL, NULL) == -1) {
        return -1;
    }

    if (syscall_error) return -1;

    return 0;
}


