#ifndef HZ_SERVER_HTTP_REQUEST_H
#define HZ_SERVER_HTTP_REQUEST_H

#include <glob.h>
#include "list.h"
#include <time.h>
#include "other.h"

#define HTTP_PARSE_INVALID_METHOD        10
#define HTTP_PARSE_INVALID_REQUEST       11
#define HTTP_PARSE_INVALID_HEADER        12

#define HTTP_UNKNOWN                     0x0001
#define HTTP_GET                         0x0002
#define HTTP_HEAD                        0x0004
#define HTTP_POST                        0x0008

#define SUCCESS                          1
#define HTTP_OK                          200

#define HTTP_NOT_MODIFIED                304

#define HTTP_NOT_FOUND                   404


#define MAX_BUF 8124
struct http_request
{
    void *root;
    int fd;
    char buf[MAX_BUF];
    //只有post时有用
    char contype[256];      // 请求体类型
    char conlength[16];     // 请求体长度
    size_t  pos, last;
    int state;
    void *request_start;
    void *method_end;
    int method;
    void *uri_start;
    void *uri_end;
    void *path_start;
    void *path_end;
    void *query_start;
    void *query_end;

    int http_major;
    int http_minor;
    void *request_end;

    list_head list;
    void *head_key_start;
    void *head_key_end;
    void *head_value_start;
    void *head_value_end;

    void *timer;

};


struct http_out
{
    int fd;
    int keep_alive;
    time_t mtime;
    int modified;

    int status;
};


struct http_heads
{
    void *key_start, *key_end;
    void *value_start, *value_end;
    list_head list;
};

typedef int (*http_head_handler)(http_request* r, http_out *o, char *data, int len);

struct http_head_handler_array
{
    char* name;
    http_head_handler handler;
};

void http_handle_head(http_request *r, http_out *out);
void http_close_conn(http_request *r);

void init_request(http_request *r, int fd, config *cf);

void init_out(http_out *out, int fd);



extern http_head_handler_array head_array[];

const char *get_shortmsg_from_status_code(int status_code);

#endif //HZ_SERVER_HTTP_REQUEST_H
