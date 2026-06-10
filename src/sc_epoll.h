#ifndef SCWRAPPER_SC_EPOLL_H
#define SCWRAPPER_SC_EPOLL_H

#include <stdint.h>
#include <sys/epoll.h>

extern int epoll;

int sc_epoll_init();
int sc_epoll_ctl_add(int fd, uint32_t events);
int sc_epoll_ctl_remove(int fd);

#endif // SCWRAPPER_SC_EPOLL_H
