#ifndef SIGNALS_H
#define SIGNALS_H

#define _GNU_SOURCE
#include <signal.h>
#include <stdlib.h>
#include <sys/signalfd.h>
#include <sys/types.h>

int make_sigchld_fd(void);

int make_sigint_fd(void);

#endif