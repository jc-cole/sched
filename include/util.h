#ifndef UTIL_H
#define UTIL_H

#include "job.h"
#include <stdio.h>

void print_jobs_debug_temp(Job *jobs, size_t num_jobs);

void print_jobs_by_status(Job *jobs, size_t num_jobs, JobStatus matching_status);

#endif