#include <fstream>
#include "memory_pool.h"
#include "timer.h"
#include "epoll.h"
#include "other.h"
#include <signal.h>
#include <netinet/in.h>
#include <cstring>
#include <zconf.h>
#include "hz_error.h"
#include "http_request.h"
#include "thread_pool.h"
#include "http_request.h"
#include "http.h"

static bool timer_comp(void *ti, void *tj) {
    timer_nodes *timeri = (timer_nodes *)ti;
    timer_nodes *timerj = (timer_nodes *)tj;

    return (timeri->key < timerj->key)? true:false;
}
std::fstream log_file("log");
allocator hz_alloc;
timer* hz_timer = new timer(timer_comp, 10);
hz_epoll *epoll = new hz_epoll;


int main()
{
    config cf;
    cf.port = 5000;
    cf.root = (void *) ("/home/huangzhe/CLionProjects/hz_server/html");
    cf.thread_num = 4;

    signal(SIGPIPE, SIG_IGN);

    int listenfd;
    struct sockaddr_in clientaddr;
    // initialize clientaddr and inlen to solve "accept Invalid argument" bug
    socklen_t inlen = 1;
    memset(&clientaddr, 0, sizeof(struct sockaddr_in));

    listenfd = open_listenfd(cf.port);
    if (make_socket_non_blocking(listenfd) != 0)
    {
        hz_error("sockect error", "make no block fail", log_file);
    }

    struct epoll_event event;
    http_request *request = (http_request*) hz_alloc.mem_palloc(sizeof(http_request));
    init_request(request, listenfd, &cf);

    event.data.ptr = (void *)request;
    event.events = EPOLLIN | EPOLLET;

    epoll->add(listenfd, &event);

    thread_pool pool(cf.thread_num);
    int time;
    int n;
    int fd;
    while(1)
    {
        time = hz_timer->find_timer();
        n = epoll->wait(time);
        hz_timer->handle_expire_timers();

        for(int i = 0; i<n; ++i)
        {
            http_request* r = (http_request*)(epoll->get_event(i)).data.ptr;
            fd = r->fd;

            if(listenfd  == fd)
            {
                int infd;
                while(1)
                {
                    infd = accept(listenfd, (struct sockaddr *)&clientaddr, &inlen);
                    if(infd < 0)
                    {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                        {
                            break;
                        }
                        else
                        {
                            hz_error("accept error", "accept failed", log_file);
                            break;
                        }
                    }

                    make_socket_non_blocking(infd);
                    http_request* request = (http_request*)hz_alloc.mem_palloc(sizeof(http_request));
                    init_request(request, infd, &cf);

                    event.data.ptr = (void*)request;
                    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;

                    epoll->add(infd, &event);
                    hz_timer->add_timer(request, 500, http_close_conn);


                }
            }
            else
            {
                if ((epoll->get_event(i).events & EPOLLERR) || (epoll->get_event(i).events & EPOLLHUP) || (!(epoll->get_event(i).events & EPOLLIN)))
                {
                    close(fd);
                    continue;
                }
                pool.add(do_request, epoll->get_event(i).data.ptr);
            }
        }
    }

    delete hz_timer;
    delete epoll;

}
