#include "memory_pool.h"
#include "hz_error.h"
#include <fstream>
#include <cstdlib>


extern std::fstream log_file;


allocator::allocator(size_t size)
{
    memory_pool = (pool*)mem_alloc(size);
    if (memory_pool == NULL)
    {
        hz_error("memory erro","allocator failed",log_file);
    }
    else
    {
        memory_pool->d.start = (u_char*)memory_pool + sizeof(pool);
        memory_pool->d.end = (u_char*)memory_pool + size;
        memory_pool->d.next = NULL;
        memory_pool->d.failed = 0;
        size = size - sizeof(pool);
        if(size < 4095)//size of one page
        {
            memory_pool->max = size;
        }
        else
            memory_pool->max = 4095;
        memory_pool->current = memory_pool;
        memory_pool->large = NULL;
        lock = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(lock,NULL);
    }
}

void* allocator::mem_alloc(size_t size)
{
    void* p;
    p = malloc(size);
    if(p == NULL)
    {
        hz_error("memory error","malloc failed",log_file);
    }
    return p;
}

void* allocator::mem_palloc(size_t size)
{
    /*
    void* p;
    p = malloc(size);
    if(p == NULL)
    {
        hz_error("memory error","malloc failed",log_file);
    }
    return p;
    */
    u_char *m;
    pool *p;

    pthread_mutex_lock(lock);
    if(size <= memory_pool->max)
    {
        p = memory_pool->current;
        do{
            m =  hz_align_ptr(p->d.start,HZ_ALIGNMENT);
            if((size_t) (p->d.end - m) >= size)
            {
                p->d.start = m+size;
                pthread_mutex_unlock(lock);
                return (void*)m;
            }

            p = p->d.next;
        }while(p);
        void* temp =  mem_palloc_block(size);
        pthread_mutex_unlock(lock);
        return temp;
    }

    void* temp = mem_palloc_large(size);
    pthread_mutex_unlock(lock);
    return temp;
}

void* allocator::mem_palloc_block(size_t size)
{
    u_char *m;
    size_t psize;
    pool *p, *new_p, *current;
    psize = (size_t)(memory_pool->d.end - (u_char* )memory_pool);
    m = (u_char *)mem_alloc(psize);
    if(m == NULL)
    {
        return NULL;
    }

    new_p = (pool*) m;
    new_p->d.end = m + psize;
    new_p->d.next = NULL;
    new_p->d.failed = 0;

    m += sizeof(pool_data);
    m = hz_align_ptr(m,HZ_ALIGNMENT);
    new_p->d.start = m+size;
    current = memory_pool->current;
    for(p = current;p->d.next; p = p->d.next)
    {
        if(p->d.failed++ > 4)
        {
            current = p->d.next;
        }
    }

    p->d.next = new_p;
    memory_pool->current = current?current:new_p;
    return (void*)m;
}


void* allocator::mem_palloc_large(size_t size)
{
    void *p;
    int n;
    large_mem *large;

    p = mem_alloc(size);
    if(p == NULL)
    {
        return NULL;
    }

    n = 0;
    for( large = memory_pool->large; large; large = large->next)
    {
        if(large->data == NULL)
        {
            large->data = p;
            return p;
        }

        if(n++ > 3)
        {
            break;
        }
    }

    large = (large_mem*)mem_alloc(sizeof(large_mem));
    if( large == NULL)
    {
        free(p);
        return NULL;
    }

    large->data = p;
    large->next = memory_pool->large;
    memory_pool->large = large;
    return p;
}

void allocator::mem_free(void *p)
{
    //free(p);

    large_mem *l;
    pthread_mutex_lock(lock);
    for(l = memory_pool->large; l; l = l->next)
    {
        if(p == l->data)
        {
            free(l->data);
            l->data = NULL;
        }
    }
    pthread_mutex_unlock(lock);

}

void allocator::reset()
{
    pool *p;
    large_mem *l;
    pthread_mutex_lock(lock);
    for(l = memory_pool->large; l; l = l->next)
    {
        if(l->data)
        {
            free(l->data);
        }
    }

    memory_pool->large = NULL;

    for(p = memory_pool; p; p = p->d.next)
    {
        p->d.start = (u_char*) p +sizeof(pool);
    }
    pthread_mutex_unlock(lock);
}

allocator::~allocator()
{
    pool *p, *n;
    large_mem *l;
    for(l = memory_pool->large; l; l = l->next)
    {
        if(l->data)
        {
            free(l->data);
        }
    }

    for(l = memory_pool->large; l; l = l->next)
    {
        if(l != NULL)
        {
            free(l);
        }
    }
    for(p = memory_pool, n = memory_pool->d.next; ;p = n, n = n->d.next)
    {
        free(p);
        if(n == NULL)
        {
            break;
        }
    }
    pthread_mutex_destroy(lock);
    free(lock);
}