#ifndef HZ_SERVER_HTTP_H
#define HZ_SERVER_HTTP_H

#define MAXLINE     8192
struct mime_type {
    const char *type;
    const char *value;
};

void do_request(void *request);
#endif //HZ_SERVER_HTTP_H
