# ifndef TIMER_H
# define TIMER_H

#include <sys/timerfd.h>

int reset_timer(int timerfd, int quantum_ms);

# endif