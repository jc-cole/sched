#ifndef RR_H
#define RR_H

#define _GNU_SOURCE
#include "job.h"
#include "timer.h"
#include "signals.h"
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <poll.h>
#include "util.h"

int round_robin(Job *jobs, size_t num_jobs, int quantum_ms);

#endif