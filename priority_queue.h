#ifndef HZ_SERVER_PRIORITY_QUEUE_H
#define HZ_SERVER_PRIORITY_QUEUE_H

#include <cstdlib>
typedef bool (*cmp)(void *p1, void *p2);
class hz_pq_queue
{

private:
    void **pq = NULL;
    size_t nalloc;
    size_t size;
    cmp copmare;
    bool resize(size_t new_size);
    void exchange(size_t i, size_t j);
    void pq_sink(size_t i);
    void pq_swim(size_t i);

public:
    hz_pq_queue(cmp compare, size_t size);
    bool pa_is_empty();
    size_t pq_size();
    void* pq_min();
    bool pq_delete_min();
    bool pq_intsert(void *);

};
#endif //HZ_SERVER_PRIORITY_QUEUE_H
