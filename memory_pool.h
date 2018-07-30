#ifndef HZ_SERVER_MEMORY_POOL_H
#define HZ_SERVER_MEMORY_POOL_H

#include <cstdlib>
#include <pthread.h>
struct pool;
#define HZ_ALIGNMENT sizeof(unsigned long)
#define hz_align_ptr(p, a)  \
        (u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))

struct large_mem
{
    large_mem *next;
    void *data;
};

struct pool_data
{
    u_char *start;
    u_char *end;
    pool *next;
    unsigned int failed = 0;

};

struct  pool
{
    pool_data d;
    size_t max;
    pool *current;
    large_mem *large;
};

class allocator
{
public:
    allocator(size_t size = 4096);
    ~allocator();
    void *mem_palloc(size_t size);
    void mem_free(void* p);
    void reset();
    pthread_mutex_t *lock;
private:
    pool *memory_pool = NULL;
    void *mem_alloc(size_t size);
    void *mem_palloc_block(size_t size);
    void *mem_palloc_large(size_t size);



};
#endif //HZ_SERVER_MEMORY_POOL_H
