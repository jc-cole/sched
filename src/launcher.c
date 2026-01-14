#define _GNU_SOURCE

#include "job.h"
#include "launcher.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

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

static JobStatus interpret_status(int st) {
    if (WIFEXITED(st)) {
        return EXITED;
    } else if (WIFSIGNALED(st)) {
        return TERMINATED;
    } else if (WIFSTOPPED(st)) {
        return STOPPED;
    } else {
        return LAUNCH_FAILURE;
    }
}


int launch_job(Job *job) {
    int pipe[2];
    pipe2(pipe, O_CLOEXEC);
    pid_t pid = fork();
    if (pid == 0) {
        close(pipe[0]);
        
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

        int st;
        if (waitpid(pid, &st, WUNTRACED) == -1) {
            job->status = LAUNCH_FAILURE;
            return -1;
        }

        JobStatus pre_exec_status = interpret_status(st);
        if (pre_exec_status != STOPPED) {
            job->status = LAUNCH_FAILURE;
            return -1;
        }

        if (kill(pid, SIGCONT) == -1) {
            job->status = LAUNCH_FAILURE;
            return -1;
        }

        char marker;
        ssize_t n = read(pipe[0], &marker, 1);
        if (n <= 0) {
            job->status = LAUNCH_FAILURE;
            return -1;
        } else if (marker != 'M') {
            job->status = EXEC_FAILURE;
            return 1;
        }

        int err;
        size_t got;
        while (got < sizeof(err)) {
            ssize_t r = read(pipe[0], (char*) &err + got, sizeof(err) - got);

            if (r == 0) {
                // got marker, no errno -> success
                break;
            }

            if (r < 0) {
                printf("it's this launch failure\n");
                job->status = LAUNCH_FAILURE;
                return -1;
            }

            got += (size_t)r;
        }

        if (got == sizeof(err)) {
            job->status = EXEC_FAILURE;
            return 1;
        }

        if (kill(pid, SIGSTOP) == -1) {
            job->status = LAUNCH_FAILURE;
            return -1;
        }

        if (waitpid(pid, &st, WUNTRACED) == -1) {
            job->status = LAUNCH_FAILURE;
            return -1;
        }

        JobStatus post_exec_status = interpret_status(st);
        if (pre_exec_status == LAUNCH_FAILURE) {
            job->status = LAUNCH_FAILURE;
            return -1;
        } else {
            job->status = post_exec_status;
            return 0;
        }
    }
}