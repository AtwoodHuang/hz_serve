#include "memory_pool.h"
#include "priority_queue.h"
#include "hz_error.h"
#include <fstream>
#include <cstring>

extern allocator hz_alloc;
extern std::fstream log_file;


hz_pq_queue::hz_pq_queue(cmp comp, size_t size_i) :copmare(comp), size(size_i+1), nalloc(0)
{
    pq = (void**) hz_alloc.mem_palloc(sizeof(void*) * (size+1));
    if(!pq)
    {
        hz_error("qp_error", "pq init failed", log_file);
    }

}

bool hz_pq_queue::pa_is_empty()
{
    if(nalloc == 0)
        return true;
    else
        return false;
}

size_t hz_pq_queue::pq_size()
{
    return nalloc;
}

void* hz_pq_queue::pq_min()
{
    if(nalloc == 0)
        return NULL;
    return pq[1];
}

bool hz_pq_queue::resize(size_t new_size)
{
   if(new_size <= nalloc)
   {
       hz_error("qp_error", "new size too small", log_file);
       return false;
   }

    void **new_ptr = (void **)malloc(sizeof(void* )* new_size);
    if(!new_ptr)
    {
        hz_error("qp_error", "pq malloc failed", log_file);
        return -1;
    }
    memcpy(new_ptr, pq, sizeof(void*) * (nalloc+1));
    hz_alloc.mem_free((void*) pq);
    pq = new_ptr;
    size = new_size;
    return true;
}

void hz_pq_queue::exchange(size_t i, size_t j)
{
    void* tmp = pq[i];
    pq[i] = pq[j];
    pq[j] = tmp;
}

void hz_pq_queue::pq_swim(size_t k)
{
    while(k > 1 && copmare(pq[k], pq[k/2]))
    {
        exchange(k,k/2);
        k = k/2;
    }

}

void hz_pq_queue::pq_sink(size_t i)
{
    size_t j;
    size_t nalloc2= this->nalloc;
    while(2*i <= nalloc2)
    {
        j = 2*i;
        if(j < nalloc2 && copmare(pq[j+1], pq[j]))
            j++;
        if(!copmare(pq[j], pq[i]))
            break;
        exchange(j, i);
        i = j;
    }
}

bool hz_pq_queue::pq_delete_min()
{
    if(nalloc == 0)
        return true;
    exchange(1, nalloc);
    nalloc-- ;
    pq_sink(1);
    if (nalloc > 0 && nalloc <= (size - 1)/4)
    {
        if (resize(size / 2) < 0) {
            return false;
        }
    }

    return true;
}

bool hz_pq_queue::pq_intsert(void *item)
{
    if(nalloc +1 == size)
    {
        if(resize(size * 2 ) < 0)
        {
            return false;
        }
    }

    pq[++nalloc] = item;
    pq_swim(nalloc);
    return true;
}


