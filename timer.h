#ifndef HZ_SERVER_TIMER_H
#define HZ_SERVER_TIMER_H

#include "priority_queue.h"
#include "http_request.h"
#include <pthread.h>
typedef void (*timer_handler_pt)(http_request *rq);

struct timer_nodes
{
    size_t key;
    bool deleted = false;
    timer_handler_pt handler = NULL;
    http_request* rq = NULL;
};

class timer
{
public:
    timer(cmp compare, size_t size);
    int find_timer();
    void handle_expire_timers();
    void add_timer(http_request* rq, size_t timeout, timer_handler_pt handler);
    void delete_timer(http_request* rq);
    ~timer();
    pthread_mutex_t* lock;


private:
    hz_pq_queue queue;
    size_t current_msec;

    void time_update();
};
#endif //HZ_SERVER_TIMER_H
