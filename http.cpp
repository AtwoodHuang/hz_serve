#include "http.h"
#include "cstdlib"
#include "http_request.h"
#include <sys/stat.h>
#include "timer.h"
#include "other.h"
#include <unistd.h>
#include <errno.h>
#include "hz_error.h"
#include <fstream>
#include <sys/epoll.h>
#include "http_parse.h"
#include "memory_pool.h"
#include "epoll.h"
#include <cstring>
#include "robust_io.h"
#include<fcntl.h>
#include <sys/mman.h>
#include "hz_error.h"
#include "dynamic_serve.h"

extern std::fstream log_file;
extern allocator hz_alloc;
extern hz_epoll* epoll;
extern timer* hz_timer;

static const char* get_file_type(const char *type);
static void parse_uri(char *uri, int length, char *filename, char*name, char *querystring);
static void do_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
static void serve_static(int fd, char *filename, size_t filesize, http_out *out);
static void serve_dynamic(php_parama* parama, http_out *out, http_request* p);
static char *ROOT = NULL;

mime_type mimes[] =
        {
                {".html", "text/html"},
                {".xml", "text/xml"},
                {".xhtml", "application/xhtml+xml"},
                {".txt", "text/plain"},
                {".rtf", "application/rtf"},
                {".pdf", "application/pdf"},
                {".word", "application/msword"},
                {".png", "image/png"},
                {".gif", "image/gif"},
                {".jpg", "image/jpeg"},
                {".jpeg", "image/jpeg"},
                {".au", "audio/basic"},
                {".mpeg", "video/mpeg"},
                {".mpg", "video/mpeg"},
                {".avi", "video/x-msvideo"},
                {".gz", "application/x-gzip"},
                {".tar", "application/x-tar"},
                {".css", "text/css"},
                {NULL ,"text/plain"}
        };




void do_request(void *ptr)
{
    http_request *r = (http_request *)ptr;
    int fd = r->fd;
    int rc, n;
    char filename[512];
    char name[512];
    struct stat sbuf;
    ROOT = (char*)r->root;
    char *plast = NULL;
    size_t remain_size;

    hz_timer->delete_timer(r);
    for(;;)
    {
        plast = &r->buf[r->last % MAX_BUF];
        remain_size = MIN(MAX_BUF - (r->last - r->pos) - 1, MAX_BUF - r->last % MAX_BUF);

        n = read(fd, plast, remain_size);

        if (n == 0)
        {
            // EOF
            http_close_conn(r);
            return;
        }

        if (n < 0)
        {
            if (errno != EAGAIN)
            {
                hz_error("read err", errno, log_file);
                http_close_conn(r);
                return;
            }
            break;
        }

        r->last += n;
        if(r->last - r->pos >= MAX_BUF)
            hz_error("request error", "request buffer overflow", log_file);

        rc = http_parse_request_line(r);
        if (rc == EAGAIN)
        {
            continue;
        }
        else if (rc != SUCCESS)
        {
            hz_error("request error", "request parse failed", log_file);
            http_close_conn(r);
            return;
        }

        rc = http_parse_request_body(r);
        if (rc == EAGAIN)
        {
            continue;
        }
        else if (rc != SUCCESS)
        {
            hz_error("request error", "line parse failed", log_file);
            http_close_conn(r);
            return;
        }

        /*
        *   handle http header
        */
        http_out *out = (http_out *)hz_alloc.mem_palloc(sizeof(http_out));
        if (out == NULL) {
            hz_error("request error", "memory not enough", log_file);
            exit(1);
        }

        init_out(out, fd);
        char querystring[50];
        parse_uri((char*)r->uri_start, (char*)r->uri_end - (char*)r->uri_start, filename, name, querystring);

        if(stat(filename, &sbuf) < 0) {
            do_error(fd, filename, "404", "Not Found", "hz_server can't find the file");
            continue;
        }

        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
        {
            do_error(fd, filename, "403", "Forbidden", "hz_server can't read the file");
            continue;
        }

        out->mtime = sbuf.st_mtime;

        http_handle_head(r, out);

        if (out->status == 0)
        {
            out->status = HTTP_OK;
        }
        char* query = strstr(filename, ".php");
        if(query)
        {
            php_parama* parama =  (php_parama*)malloc(sizeof(php_parama));
            memset(parama, 0, sizeof(php_parama));
            init_php_parama(r, parama, filename, name, querystring);
            serve_dynamic(parama, out, r);
        }
        else
        {
            serve_static(fd, filename, sbuf.st_size, out);
        }
        if (!out->keep_alive)
        {
            hz_alloc.mem_free((void*)out);
            http_close_conn(r);
            return;
        }
        hz_alloc.mem_free((void*)out);

    }

    struct epoll_event event;
    event.data.ptr = ptr;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;

    epoll->mode(r->fd, &event);
    hz_timer->add_timer(r, 500, http_close_conn);
    return;
}

static void parse_uri(char *uri, int uri_length, char *filename, char* name, char *querystring) {
    if(uri == NULL)
    {
        hz_error("parse error", "uri is NULL", log_file);
    }
    uri[uri_length] = '\0';

    char *question_mark = strchr(uri, '?');
    int file_length;
    if (question_mark)
    {
        file_length = (int)(question_mark - uri);
        strcpy(querystring, question_mark+1);
    }
    else
    {
        file_length = uri_length;
    }

    strcpy(filename, ROOT);
    strncpy(name, uri, file_length);
    name[file_length] = '\0';


    if (uri_length > (512 >> 1))
    {
        hz_error("parse error", "", log_file);
        return;
    }
    strncat(filename, uri, file_length);

    char *last_comp = strrchr(filename, '/');
    char *last_dot = strrchr(last_comp, '.');
    if (last_dot == NULL && filename[strlen(filename)-1] != '/') {
        strcat(filename, "/");
    }

    if(filename[strlen(filename)-1] == '/') {
        strcat(filename, "index.html");
    }
    return;
}

static void do_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char header[MAXLINE], body[MAXLINE];

    sprintf(body, "<html><title>hz_server Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\n", body);
    sprintf(body, "%s%s: %s\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\n</p>", body, longmsg, cause);
    sprintf(body, "%s<hr><em>web server</em>\n</body></html>", body);

    sprintf(header, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
    sprintf(header, "%sServer: hz_server\r\n", header);
    sprintf(header, "%sContent-type: text/html\r\n", header);
    sprintf(header, "%sConnection: close\r\n", header);
    sprintf(header, "%sContent-length: %d\r\n\r\n", header, (int)strlen(body));
    rio_writen(fd, header, strlen(header));
    rio_writen(fd, body, strlen(body));
    return;
}

static void serve_static(int fd, char *filename, size_t filesize, http_out *out) {
    char header[MAXLINE];
    char buf[512];
    size_t n;
    struct tm tm;

    const char *file_type;
    const char *dot_pos = strrchr(filename, '.');
    file_type = get_file_type(dot_pos);

    sprintf(header, "HTTP/1.1 %d %s\r\n", out->status, get_shortmsg_from_status_code(out->status));

    if (out->keep_alive) {
        sprintf(header, "%sConnection: keep-alive\r\n", header);
        sprintf(header, "%sKeep-Alive: timeout=%d\r\n", header, 500);
    }

    if (out->modified) {
        sprintf(header, "%sContent-type: %s\r\n", header, file_type);
        sprintf(header, "%sContent-length: %zu\r\n", header, filesize);
        localtime_r(&(out->mtime), &tm);
        strftime(buf, 512,  "%a, %d %b %Y %H:%M:%S GMT", &tm);
        sprintf(header, "%sLast-Modified: %s\r\n", header, buf);
    }

    sprintf(header, "%sServer: hz_server\r\n", header);
    sprintf(header, "%s\r\n", header);

    n = (size_t)rio_writen(fd, header, strlen(header));
    if (n != strlen(header))
    {
        return;
    }

    if (!out->modified)
    {
        return;
    }

    int srcfd = open(filename, O_RDONLY, 0);
    // can use sendfile
    char *srcaddr = (char*)mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    close(srcfd);

    n = rio_writen(fd, srcaddr, filesize);


    munmap(srcaddr, filesize);
    return;
}

static void serve_dynamic(php_parama* parama, http_out *out, http_request* p)
{
    int sock;

    // 创建一个连接到fastcgi服务器的套接字
    sock = open_fpm();

    // 发送http请求数据
    send_fastcgi(parama, sock, p);

    // 接收处理结果
    recv_fastcgi(p->fd, sock);

    close(sock); // 关闭与fastcgi服务器连接的套接字
    return;
}

static const char* get_file_type(const char *type)
{
    if (type == NULL) {
        return "text/plain";
    }

    int i;
    for (i = 0; mimes[i].type != NULL; ++i) {
        if (strcmp(type, mimes[i].type) == 0)
            return mimes[i].value;
    }
    return mimes[i].value;
}
