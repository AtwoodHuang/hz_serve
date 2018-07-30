#include "timer.h"
#include <sys/time.h>
#include "hz_error.h"
#include <fstream>
#include "memory_pool.h"

extern std::fstream log_file;
extern allocator hz_alloc;


static bool timer_comp(void *ti, void *tj) {
    timer_nodes *timeri = (timer_nodes *)ti;
    timer_nodes *timerj = (timer_nodes *)tj;

    return (timeri->key < timerj->key)? true: false;
}

timer::~timer()
{
    pthread_mutex_destroy(lock);
    ::free(lock);
}

void timer::time_update()
{
    struct timeval tv;
    int rc;

    rc = gettimeofday(&tv, NULL);
    if(rc != 0)
    {
        hz_error("time error", "get time of day error", log_file);
    }

    current_msec = tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

timer::timer(cmp compare, size_t size) :queue(compare, size)
{
    lock = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(lock,NULL);
    time_update();
}

int timer::find_timer()
{
    timer_nodes *timer_node;
    int time = -1;
    pthread_mutex_lock(lock);
    while(!queue.pa_is_empty())
    {
        time_update();
        timer_node = (timer_nodes*)queue.pq_min();
        if(timer_node == NULL)
        {
            hz_error("time error", "min error", log_file);
        }

        if(timer_node->deleted)
        {
            if(!queue.pq_delete_min())
            {
                hz_error("time error", "min error", log_file);
            }
            hz_alloc.mem_free((void*)timer_node);
            continue;
        }

        time = (int ) (timer_node->key - current_msec);
        time = (time > 0? time: 0);
        break;
    }
    pthread_mutex_unlock(lock);
    return time;

}


void timer::handle_expire_timers()
{
    timer_nodes *timer_node;
    pthread_mutex_lock(lock);
    while(!queue.pa_is_empty())
    {
        time_update();
        timer_node = (timer_nodes*)queue.pq_min();
        if(timer_node == NULL)
        {
            hz_error("time error", "min error", log_file);
        }

        if(timer_node->deleted)
        {
            if(!queue.pq_delete_min())
            {
                hz_error("time error", "min error", log_file);
            }
            hz_alloc.mem_free((void*)timer_node);
            continue;
        }

        if(timer_node->key > current_msec)
        {
            pthread_mutex_unlock(lock);
            return;
        }

        if(timer_node->handler)
        {
            timer_node->handler(timer_node->rq);
        }

        if(!queue.pq_delete_min())
        {
            hz_error("time error", "min error", log_file);
        }

        hz_alloc.mem_free((void*)timer_node);
    }
    pthread_mutex_unlock(lock);
}


void timer::add_timer(http_request *rq, size_t timeout, timer_handler_pt handler)
{
    pthread_mutex_lock(lock);
    timer_nodes *timer_node = (timer_nodes*)hz_alloc.mem_palloc(sizeof(timer_nodes));
    if(timer_node == NULL)
    {
        hz_error("time error", "time add error", log_file);
    }

    time_update();
    rq->timer = timer_node;
    timer_node->key = current_msec + timeout;
    timer_node->deleted = false;
    timer_node->handler = handler;
    timer_node->rq = rq;

    if(!queue.pq_intsert((void*) timer_node))
    {
        hz_error("time error", "time add error", log_file);
    }
    pthread_mutex_unlock(lock);
}

void timer::delete_timer(http_request *rq)
{
    pthread_mutex_lock(lock);
    time_update();
    timer_nodes* timer_node = (timer_nodes*)rq->timer;
    if(timer_node == NULL)
    {
        hz_error("time error", "time is NULL", log_file);
    }

    timer_node->deleted = true;
    pthread_mutex_unlock(lock);
}