#ifndef REAP_H
#define REAP_H

#include "job.h"
#include "util.h"
#include <wait.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

int reap_and_update(Job *jobs, size_t num_jobs, size_t *active_jobs, int* reported_statuses_debug);

#endif