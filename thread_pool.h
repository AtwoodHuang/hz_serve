#ifndef HZ_SERVER_THREAD_POOL_H
#define HZ_SERVER_THREAD_POOL_H
#include <cstdlib>
#include <pthread.h>

struct tasks
{
    void (*func)(void*);
    void* arg;
    tasks* next;
};


class thread_pool
{
public:
    thread_pool(int count = 8);
    void add(void (*func)(void *), void *arg);
    ~thread_pool();
    pthread_mutex_t *lock;
    pthread_cond_t *cond;

private:

    pthread_t *threads;
    tasks *head;
    int thread_count;
    int queue_size;
    int started;
    bool shutdown;
    static void* worker(void* arg);
    void free();
};
#endif //HZ_SERVER_THREAD_POOL_H
