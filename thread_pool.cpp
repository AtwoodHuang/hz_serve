#include "thread_pool.h"
#include "memory_pool.h"
#include "hz_error.h"
#include <fstream>


extern allocator hz_alloc;
extern std::fstream log_file;

thread_pool::thread_pool(int count) {
    if (count <= 0) {
        hz_error("thread error", "thread num is zero", log_file);
    }

    thread_count = 0;
    queue_size = 0;
    started = 0;
    shutdown = false;
    threads = (pthread_t *) hz_alloc.mem_palloc(sizeof(pthread_t) * count);
    head = (tasks *) hz_alloc.mem_palloc(sizeof(tasks));

    head->func = NULL;
    head->arg = NULL;
    head->next = NULL;

    lock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
    pthread_mutex_init(lock, NULL);

    pthread_cond_init(cond, NULL);

    for (int i = 0; i < count; ++i)
    {
        if(pthread_create(&threads[i], NULL, worker, (void*)this) != 0)
        {
            hz_error("thread error", "thread cread failed", log_file);
        }
        thread_count++;
        started++;
    }
}


void thread_pool::add(void (*func)(void *), void *arg)
{
    if(pthread_mutex_lock(lock) != 0)
    {
        hz_error("thread error", "lock failed", log_file);
        return;
    }


    tasks* task = (tasks*) hz_alloc.mem_palloc(sizeof(tasks));

    task->func = func;
    task->arg = arg;
    task->next = head->next;
    head->next = task;

    queue_size++;

    pthread_cond_signal(cond);

    pthread_mutex_unlock(lock);
    return;
}


void thread_pool::free()
{
    if(started > 0)
    {
        hz_error("thread erro", "the thread is still started", log_file);
        return ;
    }

    if(threads)
        hz_alloc.mem_free((void*) threads);

    tasks* old;

    while(head->next)
    {
        old = head->next;
        head->next = head->next->next;
        hz_alloc.mem_free((void*) old);
    }

    return ;
}


thread_pool::~thread_pool()
{
    pthread_mutex_lock(lock);
    shutdown = true;

    pthread_cond_broadcast(cond);

    pthread_mutex_unlock(lock);

    for(int i = 0; i<thread_count; i++)
    {
        if(pthread_join(threads[i], NULL) != 0)
        {
            hz_error("thread error", "pthread_join failed", log_file);
        }
    }

    pthread_mutex_destroy(lock);
    pthread_cond_destroy(cond);
    ::free(lock);
    ::free(cond);
    free();

}


void* thread_pool::worker(void *arg)
{
    if(arg == NULL)
    {
        hz_error("thread error", "pthread_join failed", log_file);
        return NULL;
    }

    thread_pool *pool = (thread_pool*) arg;
    tasks* task;

    while(1)
    {
        pthread_mutex_lock(pool->lock);
        while((pool->queue_size == 0) && !(pool->shutdown))
        {
            pthread_cond_wait((pool->cond), (pool->lock));
        }

        if(pool->shutdown)
            break;

        task = pool->head->next;
        if(task == NULL)
        {
            pthread_mutex_unlock((pool->lock));
            continue;
        }

        pool->head->next = task->next;
        pool->queue_size--;

        pthread_mutex_unlock((pool->lock));

        (*(task->func))(task->arg);


        hz_alloc.mem_free((void*) task);
    }

    pool->started--;
    pthread_mutex_unlock((pool->lock));
    pthread_exit(NULL);

    return NULL;

}



