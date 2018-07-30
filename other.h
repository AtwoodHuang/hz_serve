#ifndef HZ_SERVER_UTIL_H
#define HZ_SERVER_UTIL_H

struct config
{
    void *root;
    int port;
    int thread_num;
};


int open_listenfd(int port);
int make_socket_non_blocking(int fd);

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif //HZ_SERVER_UTIL_H
