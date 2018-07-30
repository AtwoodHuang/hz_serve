#ifndef HZ_SERVER_ROBUST_IO_H
#define HZ_SERVER_ROBUST_IO_H

#include <sys/types.h>
ssize_t rio_readn(int fd, void *usrbuf, size_t n);
ssize_t rio_writen(int fd, void *usrbuf, size_t n);

#endif //HZ_SERVER_ROBUST_IO_H
