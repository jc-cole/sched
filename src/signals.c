#include "signals.h"

int make_sigchld_fd(void) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);

    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
        return -1;
    }

    int sfd = signalfd(-1, &mask, 0);

    return sfd;
}