#include "http_request.h"
#include "http_parse.h"
#include "hz_error.h"
#include <errno.h>
#include <fstream>
#include "memory_pool.h"


extern allocator hz_alloc;
extern std::fstream log_file;

int http_parse_request_line(http_request *r) {
    u_char ch, *p, *m;
    size_t pi;

    enum http_state{
        sw_start = 0,
        sw_method,
        sw_spaces_before_uri,
        sw_after_slash_in_uri,
        sw_http,
        sw_http_H,
        sw_http_HT,
        sw_http_HTT,
        sw_http_HTTP,
        sw_first_major_digit,
        sw_major_digit,
        sw_first_minor_digit,
        sw_minor_digit,
        sw_spaces_after_digit,
        sw_almost_done
    }  state;

    state = (http_state)r->state;

    for (pi = r->pos; pi < r->last; pi++)
    {
        p = (u_char *)&r->buf[pi % MAX_BUF];
        ch = *p;

        switch (state)
        {

            /* HTTP methods: GET, HEAD, POST */
            case sw_start:
                r->request_start = p;

                if (ch == CR || ch == LF)
                {
                    break;
                }

                if ((ch < 'A' || ch > 'Z') && ch != '_')
                {
                    return HTTP_PARSE_INVALID_METHOD;
                }

                state = sw_method;
                break;

            case sw_method:
                if (ch == ' ')
                {
                    r->method_end = p;
                    m = (u_char*)(r->request_start);

                    switch (p - m)
                    {

                        case 3:
                            if (str3_cmp(m, 'G', 'E', 'T', ' '))
                            {
                                r->method = HTTP_GET;
                                break;
                            }

                            break;

                        case 4:
                            if (str3_cmp(m, 'P', 'O', 'S', 'T'))
                            {
                                r->method = HTTP_POST;
                                break;
                            }

                            if (str3_cmp(m, 'H', 'E', 'A', 'D'))
                            {
                                r->method = HTTP_HEAD;
                                break;
                            }

                            break;
                        default:
                            r->method = HTTP_UNKNOWN;
                            break;
                    }
                    state = sw_spaces_before_uri;
                    break;
                }

                if ((ch < 'A' || ch > 'Z') && ch != '_')
                {
                    return HTTP_PARSE_INVALID_METHOD;
                }

                break;

                /* space* before URI */
            case sw_spaces_before_uri:

                if (ch == '/')
                {
                    r->uri_start = p;
                    state = sw_after_slash_in_uri;
                    break;
                }

                switch (ch)
                {
                    case ' ':
                        break;
                    default:
                        return HTTP_PARSE_INVALID_REQUEST;
                }
                break;

            case sw_after_slash_in_uri:

                switch (ch)
                {
                    case ' ':
                        r->uri_end = p;
                        state = sw_http;
                        break;
                    default:
                        break;
                }
                break;

                /* space+ after URI */
            case sw_http:
                switch (ch)
                {
                    case ' ':
                        break;
                    case 'H':
                        state = sw_http_H;
                        break;
                    default:
                        return HTTP_PARSE_INVALID_REQUEST;
                }
                break;

            case sw_http_H:
                switch (ch)
                {
                    case 'T':
                        state = sw_http_HT;
                        break;
                    default:
                        return HTTP_PARSE_INVALID_REQUEST;
                }
                break;

            case sw_http_HT:
                switch (ch)
                {
                    case 'T':
                        state = sw_http_HTT;
                        break;
                    default:
                        return HTTP_PARSE_INVALID_REQUEST;
                }
                break;

            case sw_http_HTT:
                switch (ch)
                {
                    case 'P':
                        state = sw_http_HTTP;
                        break;
                    default:
                        return HTTP_PARSE_INVALID_REQUEST;
                }
                break;

            case sw_http_HTTP:
                switch (ch)
                {
                    case '/':
                        state = sw_first_major_digit;
                        break;
                    default:
                        return HTTP_PARSE_INVALID_REQUEST;
                }
                break;

                /* first digit of major HTTP version */
            case sw_first_major_digit:
                if (ch < '1' || ch > '9')
                {
                    return HTTP_PARSE_INVALID_REQUEST;
                }

                r->http_major = ch - '0';
                state = sw_major_digit;
                break;

                /* major HTTP version or dot */
            case sw_major_digit:
                if (ch == '.')
                {
                    state = sw_first_minor_digit;
                    break;
                }

                if (ch < '0' || ch > '9')
                {
                    return HTTP_PARSE_INVALID_REQUEST;
                }

                r->http_major = r->http_major * 10 + ch - '0';
                break;

                /* first digit of minor HTTP version */
            case sw_first_minor_digit:
                if (ch < '0' || ch > '9')
                {
                    return HTTP_PARSE_INVALID_REQUEST;
                }

                r->http_minor = ch - '0';
                state = sw_minor_digit;
                break;

                /* minor HTTP version or end of request line */
            case sw_minor_digit:
                if (ch == CR)
                {
                    state = sw_almost_done;
                    break;
                }

                if (ch == LF)
                {
                    goto done;
                }

                if (ch == ' ')
                {
                    state = sw_spaces_after_digit;
                    break;
                }

                if (ch < '0' || ch > '9')
                {
                    return HTTP_PARSE_INVALID_REQUEST;
                }

                r->http_minor = r->http_minor * 10 + ch - '0';
                break;

            case sw_spaces_after_digit:
                switch (ch)
                {
                    case ' ':
                        break;
                    case CR:
                        state = sw_almost_done;
                        break;
                    case LF:
                        goto done;
                    default:
                        return HTTP_PARSE_INVALID_REQUEST;
                }
                break;

                /* end of request line */
            case sw_almost_done:
                r->request_end = p - 1;
                switch (ch) {
                    case LF:
                        goto done;
                    default:
                        return HTTP_PARSE_INVALID_REQUEST;
                }
        }
    }

    r->pos = pi;
    r->state = (int)state;

    return EAGAIN;

    done:

    r->pos = pi + 1;

    if (r->request_end == NULL)
    {
        r->request_end = p;
    }

    r->state = sw_start;

    return SUCCESS;
}

int http_parse_request_body(http_request *r)
{
    u_char ch, *p;
    size_t pi;

    enum parse_state{
        sw_start = 0,
        sw_key,
        sw_spaces_before_colon,
        sw_spaces_after_colon,
        sw_value,
        sw_cr,
        sw_crlf,
        sw_crlfcr
    } state;

    state = (parse_state)r->state;
    http_heads *hd;
    for (pi = r->pos; pi < r->last; pi++)
    {
        p = (u_char *)&r->buf[pi % MAX_BUF];
        ch = *p;

        switch (state)
        {
            case sw_start:
                if (ch == CR || ch == LF)
                {
                    break;
                }

                r->head_key_start = p;
                state = sw_key;
                break;
            case sw_key:
                if (ch == ' ')
                {
                    r->head_key_end = p;
                    state = sw_spaces_before_colon;
                    break;
                }

                if (ch == ':') {
                    r->head_key_end = p;
                    state = sw_spaces_after_colon;
                    break;
                }

                break;
            case sw_spaces_before_colon:
                if (ch == ' ')
                {
                    break;
                } else if (ch == ':')
                {
                    state = sw_spaces_after_colon;
                    break;
                } else
                {
                    return HTTP_PARSE_INVALID_HEADER;
                }
            case sw_spaces_after_colon:
                if (ch == ' ')
                {
                    break;
                }

                state = sw_value;
                r->head_value_start = p;
                break;
            case sw_value:
                if (ch == CR)
                {
                    r->head_value_end = p;
                    state = sw_cr;
                }

                if (ch == LF)
                {
                    r->head_value_end = p;
                    state = sw_crlf;
                }

                break;
            case sw_cr:
                if (ch == LF)
                {
                    state = sw_crlf;
                    hd = (http_heads *)hz_alloc.mem_palloc(sizeof(http_heads));
                    hd->key_start   = r->head_key_start;
                    hd->key_end     = r->head_key_end;
                    hd->value_start = r->head_value_start;
                    hd->value_end   = r->head_value_end;

                    list_add(&(hd->list), &(r->list));

                    break;
                } else
                {
                    return HTTP_PARSE_INVALID_HEADER;
                }

            case sw_crlf:
                if (ch == CR)
                {
                    state = sw_crlfcr;
                } else
                {
                    r->head_key_start = p;
                    state = sw_key;
                }
                break;

            case sw_crlfcr:
                switch (ch)
                {
                    case LF:
                        goto done;
                    default:
                        return HTTP_PARSE_INVALID_HEADER;
                }
                break;
        }
    }

    r->pos = pi;
    r->state = (int)state;

    return EAGAIN;

    done:
    r->pos = pi + 1;

    r->state = sw_start;

    return SUCCESS;
}

