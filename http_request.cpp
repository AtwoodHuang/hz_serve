#include "hz_error.h"
#include "memory_pool.h"
#include <fstream>
#include "http_request.h"
#include "other.h"
#include <cstring>
#include <unistd.h>
#include <cmath>

extern allocator hz_alloc;
extern std::fstream log_file;

static int http_process_ignore(http_request *r, http_out *out, char *data, int len);
static int http_process_connection(http_request *r, http_out *out, char *data, int len);
static int http_process_if_modified_since(http_request *r, http_out *out, char *data, int len);
static int http_set_type(http_request *r, http_out *out, char *data, int len);
static int http_set_length(http_request *r, http_out *out, char *data, int len);

http_head_handler_array head_array[] = {
        {"Host", http_process_ignore},
        {"Connection", http_process_connection},
        {"If-Modified-Since", http_process_if_modified_since},
        {"content-type", http_set_type},
        {"content-length", http_set_length},
        {"", http_process_ignore}
};

void init_request(http_request *r, int fd, config *cf)
{
    r->fd = fd;
    r->pos = 0;
    r->last = 0;
    r->state = 0;
    r->root = cf->root;
    INIT_LIST_HEAD(&(r->list));

}

void init_out(http_out *o, int fd)
{
    o->fd = fd;
    o->keep_alive = 0;
    o->modified = 1;
    o->status = 0;
}


void http_handle_head(http_request *r, http_out *o)
{
    list_head *pos;
    http_heads *hd;
    http_head_handler_array *header_in;
    int len;

    list_for_each(pos, &(r->list))
    {
        hd = list_entry(pos, http_heads, list);
        /* handle */

        for (header_in = head_array; strlen(header_in->name) > 0; header_in++)
        {
            if (strncmp((char*)hd->key_start, header_in->name, (char*)hd->key_end - (char*)hd->key_start) == 0)
            {
                len = (char*)hd->value_end - (char*)hd->value_start;
                (*(header_in->handler))(r, o, (char*)hd->value_start, len);
                break;
            }
        }

        list_del(pos);
        hz_alloc.mem_free((void*)hd);
    }
}


void http_close_conn(http_request *r)
{
    close(r->fd);
    hz_alloc.mem_free((void*)r);
}


static int http_process_ignore(http_request *r, http_out *out, char *data, int len)
{
    (void) r;
    (void) out;
    (void) data;
    (void) len;
    return SUCCESS;

}

static int http_process_connection(http_request *r, http_out *out, char *data, int len)
{
    (void) r;
    if (strncasecmp("keep-alive", data, len) == 0)
    {
        out->keep_alive = 1;
    }
    return SUCCESS;

}

static int http_process_if_modified_since(http_request *r, http_out *out, char *data, int len)
{
    (void) r;
    (void) len;

    struct tm tm;
    if (strptime(data, "%a, %d %b %Y %H:%M:%S GMT", &tm) == (char *)NULL) {
        return SUCCESS;
    }
    time_t client_time = mktime(&tm);

    double time_diff = difftime(out->mtime, client_time);
    if (fabs(time_diff) < 1e-6)
    {
        out->modified = 0;
        out->status = HTTP_NOT_MODIFIED;
    }

    return SUCCESS;
}

static int http_set_type(http_request *r, http_out *out, char *data, int len)
{
    strncpy(r->contype, data, len);
    r->contype[len] = '\0';
}

static int http_set_length(http_request *r, http_out *out, char *data, int len)
{
    strncpy(r->conlength, data, len);
    r->conlength[len] = '\0';
}

const char *get_shortmsg_from_status_code(int status_code)
{

    if (status_code == HTTP_OK)
    {
        return "OK";
    }

    if (status_code == HTTP_NOT_MODIFIED)
    {
        return "Not Modified";
    }

    if (status_code == HTTP_NOT_FOUND)
    {
        return "Not Found";
    }


    return "Unknown";
}







