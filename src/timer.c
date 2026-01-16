#include "timer.h"

int reset_timer(int timerfd, int quantum_ms) {
    struct itimerspec its;

    its.it_value.tv_sec = quantum_ms / 1000;
    its.it_value.tv_nsec = (quantum_ms % 1000) * 1000 * 1000;

    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;

    if (timerfd_settime(timerfd, 0, &its, NULL) == -1) {
        return -1;
    }

    return 0;
}