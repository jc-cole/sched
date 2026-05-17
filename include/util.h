#ifndef UTIL_H
#define UTIL_H

#include "job.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>

void set_verbose_output(bool enabled);

void verbose_printf(const char *format, ...);

void print_job_launch_verbose(const Job *job, pid_t pgid);

void print_jobs_summary(Job *jobs, size_t num_jobs);

void print_jobs_debug_temp(Job *jobs, size_t num_jobs);

void print_jobs_by_status(Job *jobs, size_t num_jobs, JobStatus matching_status);

#endif
