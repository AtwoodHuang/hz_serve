#include "epoll.h"
#include "memory_pool.h"
#include "hz_error.h"
#include <fstream>

extern  allocator hz_alloc;
extern  std::fstream log_file;

hz_epoll::hz_epoll(int max)
{
    maxevents = max;
    epfd = epoll_create1(0);
    if(epfd <=0 )
    {
        hz_error("epoll error", "epoll create failed", log_file);
    }
    events = (epoll_event *)hz_alloc.mem_palloc(sizeof(epoll_event) * maxevents);
    if(events == NULL)
    {
        hz_error("epoll error", "epoll event malloc failed", log_file);
    }

}

void hz_epoll::add(int fd, epoll_event *event)
{
    int rc = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, event);
    if(rc !=0 )
    {
        hz_error("epoll_error", "epoll_ctl failed", log_file);
    }
    return;
}

void hz_epoll::mode(int fd, epoll_event *event)
{
    int rc = epoll_ctl(epfd, EPOLL_CTL_MOD, fd, event);
    if(rc !=0 )
    {
        hz_error("epoll_error", "epoll_ctl failed", log_file);
    }
    return;
}

void hz_epoll::epoll_delete(int fd, epoll_event *event)
{
    int rc = epoll_ctl(epfd, EPOLL_CTL_DEL, fd, event);
    if(rc !=0 )
    {
        hz_error("epoll_error", "epoll_ctl failed", log_file);
    }
    return;
}


int hz_epoll::wait(int timeout)
{
    int n = epoll_wait(epfd, events, maxevents, timeout);
    if(n < 0)
    {
        hz_error("epoll_error", "epoll_wait failed", log_file);
    }
    return n;
}

hz_epoll::~hz_epoll()
{
    hz_alloc.mem_free((void* )events);
}



