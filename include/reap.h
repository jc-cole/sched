#ifndef REAP_H
#define REAP_H

#include "job.h"
#include "util.h"
#include <wait.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <poll.h>
#include <sys/signalfd.h>

int reap_and_update(Job *jobs, size_t num_jobs, size_t *active_job_count, int* reported_statuses_debug);

JobStatus update_job_status(Job *job, int wpid_status, size_t *active_job_count);

int sigint_protocol(Job *jobs, size_t num_jobs, struct pollfd poll_fds[3], pid_t children_pid, size_t *active_job_count);

#endif