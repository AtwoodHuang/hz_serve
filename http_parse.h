#ifndef HZ_SERVER_HTTP_PARSE_H
#define HZ_SERVER_HTTP_PARSE_H

#include "http_request.h"
#include <cstdint>

#define CR '\r'
#define LF '\n'
#define CRLFCRLF "\r\n\r\n"

int http_parse_request_line(http_request *r);
int http_parse_request_body(http_request *r);





#define str3_cmp(m, c0, c1, c2, c3)                                       \
    *(uint32_t *) m == ((c3 << 24) | (c2 << 16) | (c1 << 8) | c0)
#endif //HZ_SERVER_HTTP_PARSE_H
