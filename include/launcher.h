#ifndef LAUNCHER_H
#define LAUNCHER_H

#include "job.h"
#include "reap.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

int launch_jobs(Job *jobs, size_t num_jobs);

#endif