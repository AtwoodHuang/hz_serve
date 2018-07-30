#ifndef HZ_SERVER_EPOLL_H
#define HZ_SERVER_EPOLL_H

#include <sys/epoll.h>
#include "cstdlib"
class hz_epoll
{
public:
    hz_epoll(int max = 1024);
    void add(int fd, epoll_event* event);
    void mode(int fd, epoll_event* event);
    void epoll_delete(int fd, epoll_event* event);
    int wait(int timeout);
    struct epoll_event get_event(int n)
    {
        return events[n];
    }
    ~hz_epoll();
private:
    int maxevents;
    epoll_event *events = NULL;
    int epfd;
};
#endif //HZ_SERVER_EPOLL_H
