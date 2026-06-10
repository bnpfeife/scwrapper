#include "constants.h"
#include "sc_epoll.h"

#include <stdio.h>

int epoll = 0;

int sc_epoll_init() {
    if ((epoll = epoll_create1(0)) == -1) {
        perror("failed to create epoll file-descriptor");
        return RET_ERROR;
    }
    return RET_OKAY;
}

int sc_epoll_ctl_add(int fd, uint32_t events) {
    struct epoll_event event = {
        .data.fd = fd,
        .events  = events,
    };
    if (epoll_ctl(epoll, EPOLL_CTL_ADD, fd, &event)) {
        perror("failed to add device to epoll");
        return RET_ERROR;
    }
    return RET_OKAY;
}

int sc_epoll_ctl_remove(int fd) {
    if (epoll_ctl(epoll, EPOLL_CTL_DEL, fd, NULL)) {
        perror("failed to remove device from epoll");
        return RET_ERROR;
    }
    return RET_OKAY;
}
