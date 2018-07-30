#include "dynamic_serve.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <zconf.h>
#include "cstring"
#include "hz_error.h"
#include "dynamic_serve.h"
#include "robust_io.h"
#include "memory_pool.h"
#include "http_request.h"
#include "http.h"


extern std::fstream log_file;
extern allocator hz_alloc;
#define FCGI_HEADER_LEN 8
#define FCGI_MAX_LENGTH 0xFFFF          // 允许传输的最大数据长度65536
#define FCGI_HOST       "127.0.0.1"     // php-fpm地址
#define FCGI_PORT       9000            // php-fpm监听的端口地址



/*
 * 构造协议记录头部，返回FCGI_Header结构体
 */
FCGI_Header makeHeader(
        int type,
        int requestId,
        int contentLength,
        int paddingLength)
{
    FCGI_Header header;
    header.version = 1;
    header.type             = (unsigned char) type;
    header.requestIdB1      = (unsigned char) ((requestId     >> 8) & 0xff);
    header.requestIdB0      = (unsigned char) ((requestId         ) & 0xff);
    header.contentLengthB1  = (unsigned char) ((contentLength >> 8) & 0xff);
    header.contentLengthB0  = (unsigned char) ((contentLength     ) & 0xff);
    header.paddingLength    = (unsigned char) paddingLength;
    header.reserved         =  0;
    return header;
}

/*
 * 构造请求开始记录协议体，返回FCGI_BeginRequestBody结构体
 */
FCGI_BeginRequestBody makeBeginRequestBody(int role, int keepConn)
{
    FCGI_BeginRequestBody body;
    body.roleB1 = (unsigned char) ((role >>  8) & 0xff);
    body.roleB0 = (unsigned char) (role         & 0xff);
    body.flags  = (unsigned char) ((keepConn) ? 1 : 0); // 1为长连接，0为短连接
    memset(body.reserved, 0, sizeof(body.reserved));
    return body;
}

/*
 * 发送开始请求记录
 * 发送成功返回0
 * 出错返回-1
 */
int sendBeginRequestRecord(int fd, int requestId)
{
    int ret;
    // 构造一个FCGI_BeginRequestRecord结构
    FCGI_BeginRequestRecord beginRecord;

    beginRecord.header = makeHeader(FCGI_BEGIN_REQUEST, requestId, sizeof(beginRecord.body), 0);
    beginRecord.body = makeBeginRequestBody(FCGI_RESPONDER, 0);

    ret = rio_writen(fd, &beginRecord, sizeof(beginRecord));

    if (ret == sizeof(beginRecord)) {
        return 0;
    } else {
        return -1;
    }
}

/*
 * 发送名值对参数
 * 发送成功返回0
 * 出错返回-1
 */
int sendParamsRecord(int fd, int requestId, char *name, int nlen, char *value, int vlen)
{
    unsigned char *buf, *old;
    int ret, pl,  cl = nlen + vlen;
    cl = (nlen < 128) ? ++cl : cl + 4;
    cl = (vlen < 128) ? ++cl : cl + 4;

    // 计算填充数据长度
    pl = (cl % 8) == 0 ? 0 : 8 - (cl % 8);
    old = buf = (unsigned char *)malloc(FCGI_HEADER_LEN + cl + pl);

    FCGI_Header nvHeader = makeHeader(FCGI_PARAMS, requestId, cl, pl);
    memcpy(buf, (char *)&nvHeader, FCGI_HEADER_LEN);
    buf = buf + FCGI_HEADER_LEN;

    if (nlen < 128) { // name长度小于128字节，用一个字节保存长度
        *buf++ = (unsigned char)nlen;
    } else { // 大于等于128用4个字节保存长度
        *buf++ = (unsigned char)((nlen >> 24) | 0x80);
        *buf++ = (unsigned char)(nlen >> 16);
        *buf++ = (unsigned char)(nlen >> 8);
        *buf++ = (unsigned char)nlen;
    }

    if (vlen < 128) { // value长度小于128字节，用一个字节保存长度
        *buf++ = (unsigned char)vlen;
    } else { // 大于等于128用4个字节保存长度
        *buf++ = (unsigned char)((vlen >> 24) | 0x80);
        *buf++ = (unsigned char)(vlen >> 16);
        *buf++ = (unsigned char)(vlen >> 8);
        *buf++ = (unsigned char)vlen;
    }

    memcpy(buf, name, nlen);
    buf = buf + nlen;
    memcpy(buf, value, vlen);

    ret = rio_writen(fd, old, FCGI_HEADER_LEN + cl + pl);

    free(old);

    if (ret == (FCGI_HEADER_LEN + cl + pl)) {
        return 0;
    } else {
        return -1;
    }
}

/*
 * 发送空的params记录
 * 发送成功返回0
 * 出错返回-1
 */
int sendEmptyParamsRecord(int fd, int requestId)
{
    int ret;
    FCGI_Header nvHeader = makeHeader(FCGI_PARAMS, requestId, 0, 0);
    ret = rio_writen(fd, (char *)&nvHeader, FCGI_HEADER_LEN);

    if (ret == FCGI_HEADER_LEN) {
        return 0;
    } else {
        return -1;
    }
}

/*
 * 发送FCGI_STDIN数据
 * 发送成功返回0
 * 出错返回-1
 */
int sendStdinRecord(int fd, int requestId, char *data, int len)
{
    int cl = len, pl, ret;
    char buf[8] = {0};

    while (len > 0) {
        // 判断STDIN数据是否大于传输最大值FCGI_MAX_LENGTH
        if (len > FCGI_MAX_LENGTH) {
            cl = FCGI_MAX_LENGTH;
        }

        // 计算填充数据长度
        pl = (cl % 8) == 0 ? 0 : 8 - (cl % 8);

        FCGI_Header sinHeader = makeHeader(FCGI_STDIN, requestId, cl, pl);
        ret = rio_writen(fd, (char *)&sinHeader, FCGI_HEADER_LEN);  // 发送协议头部
        if (ret != FCGI_HEADER_LEN) {
            return -1;
        }

        ret = rio_writen(fd, data, cl); // 发送stdin数据
        if (ret != cl) {
            return -1;
        }

        if (pl > 0) {
            ret = rio_writen(fd, buf, pl); // 发送填充数据
            if (ret != pl) {
                return -1;
            }
        }

        len -= cl;
        data += cl;
    }

    return 0;
}

/*
 * 发送空的FCGI_STDIN记录
 * 发送成功返回0
 * 出错返回-1
 */
int sendEmptyStdinRecord(int fd, int requestId)
{
    int ret;
    FCGI_Header sinHeader = makeHeader(FCGI_STDIN, requestId, 0, 0);
    ret = rio_writen(fd, (char *)&sinHeader, FCGI_HEADER_LEN);

    if (ret == FCGI_HEADER_LEN) {
        return 0;
    } else {
        return -1;
    }
}



int open_fpm() {
    int sock;
    struct sockaddr_in serv_addr;

    // 创建套接字
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (-1 == sock) {
        hz_error("connect error", "php sockt failed", log_file);
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(FCGI_HOST);
    serv_addr.sin_port = htons(FCGI_PORT);

    // 连接服务器
    if(-1 == connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr))){
        hz_error("connect error", "php connect failed", log_file);
        return -1;
    }

    return sock;
}

int send_fastcgi(php_parama* parama, int sock, http_request* p)
{
    int requestId;
    char *buf;

    requestId = sock;

    // params参数名
    char *paname[] = {
            "SCRIPT_FILENAME",
            "SCRIPT_NAME",
            "REQUEST_METHOD",
            "REQUEST_URI",
            "QUERY_STRING",
            "CONTENT_TYPE",
            "CONTENT_LENGTH"
    };

    // 对应上面params参数名，具体参数值所在结构体中的偏移
    int paoffset[] = {
            offsetof(php_parama, filename),
            offsetof(php_parama, name),
            offsetof(php_parama, method),
            offsetof(php_parama, uri),
            offsetof(php_parama, cgiargs),
            offsetof(php_parama, contype),
            offsetof(php_parama, conlength)
    };
    // 发送开始请求记录
    if (sendBeginRequestRecord(sock, requestId) < 0) {
        hz_error("php error", "send begin error", log_file);
        return -1;
    }

    // 发送params参数
    int l = 7;
    for (int i = 0; i < l; i++) {
        // params参数的值不为空才发送
        if (strlen(((char*)parama + paoffset[i])) > 0) {
            if (sendParamsRecord(sock, requestId, paname[i], strlen(paname[i]),
                                 ((char*)parama + paoffset[i]),
                                 strlen(((char*)parama + paoffset[i]))) < 0) {
                hz_error("php error", "send parama error", log_file);
                return -1;
            }
        }
    }

    // 发送空的params参数
    if (sendEmptyParamsRecord(sock, requestId) < 0) {
        hz_error("php error", "send parama error", log_file);
        return -1;
    }

    // 继续读取请求体数据
    l = atoi(parama->conlength);
    if (l > 0) { // 请求体大小大于0
        buf = (char *)malloc(l + 1);
        memset(buf, '\0', l);
        strncpy(buf, p->buf+p->pos, l);

        // 发送stdin数据
        if (sendStdinRecord(sock, requestId, buf, l) < 0) {
            hz_error("php error", "send stdin error", log_file);
            free(buf);
            return -1;
        }

        free(buf);
    }

    // 发送空的stdin数据
    if (sendEmptyStdinRecord(sock, requestId) < 0) {
        hz_error("php error", "send stdin error", log_file);
        return -1;
    }

    return 0;
}

void init_php_parama(http_request *r, php_parama *p, char* filename, char* name, char* cigargs)
{
    strcpy(p->name, name);
    strcpy(p->filename, filename);
    strcpy(p->cgiargs, cigargs);
    int urilen = (char*)r->uri_end - (char*)r->uri_start;
    strncpy(p->uri, (const char*)r->uri_start, urilen);
    p->uri[urilen] = '\0';
    if(r->method == HTTP_GET)
        strcpy(p->method, "GET");
    if(r->method == HTTP_POST)
        strcpy(p->method, "POST");
    strcpy(p->version, "HTTP/1.1");
    strcpy(p->conlength, r->conlength);
    strcpy(p->contype, r->contype);

}

/*
 * 接收fastcgi返回的数据
 */
int recv_fastcgi(int fd, int sock) {
    int requestId;

    requestId = sock;
    FCGI_Header responHeader;
    FCGI_EndRequestBody endr;
    char *conBuf = NULL, *errBuf = NULL;
    int buf[8], cl, ret;
    int fcgi_rid;  // 保存fpm发送过来的request id

    int outlen = 0, errlen = 0;


    // 读取协议记录头部
    while (read(sock, &responHeader, FCGI_HEADER_LEN) > 0) {
        fcgi_rid = (int)(responHeader.requestIdB1 << 8) + (int)(responHeader.requestIdB0);
        if (responHeader.type == FCGI_STDOUT && fcgi_rid == requestId) {
            // 获取内容长度
            cl = (int)(responHeader.contentLengthB1 << 8) + (int)(responHeader.contentLengthB0);
            outlen += cl;

            // 如果不是第一次读取FCGI_STDOUT记录
            if (conBuf != NULL) {
                // 扩展空间
                conBuf = (char*)realloc(conBuf, outlen);
            } else {
                //第一次
                conBuf = (char *)malloc(cl);
            }

            ret = read(sock, conBuf, cl);
            if (ret == -1 || ret != cl) {
                printf("read fcgi_stdout record error\n");
                return -1;
            }

            // 读取填充内容（没用）
            if (responHeader.paddingLength > 0) {
                ret = read(sock, buf, responHeader.paddingLength);
                if (ret == -1 || ret != responHeader.paddingLength) {
                    printf("read fcgi_stdout padding error %d\n", responHeader.paddingLength);
                    return -1;
                }
            }
        } else if (responHeader.type == FCGI_STDERR && fcgi_rid == requestId) {
            // 获取内容长度
            cl = (int)(responHeader.contentLengthB1 << 8) + (int)(responHeader.contentLengthB0);
            errlen += cl;

            if (errBuf != NULL) {
                // 扩展空间
                errBuf = (char*)realloc(errBuf, errlen);
            } else {
                errBuf = (char *)malloc(cl);
            }

            ret = read(sock, errBuf, cl);
            if (ret == -1 || ret != cl) {
                return -1;
            }

            // 读取填充内容（没用）
            if (responHeader.paddingLength > 0) {
                ret = read(sock, buf, responHeader.paddingLength);
                if (ret == -1 || ret != responHeader.paddingLength) {
                    return -1;
                }
            }
        } else if (responHeader.type == FCGI_END_REQUEST && fcgi_rid == requestId) {
            // 读取结束请求协议体
            ret = read(sock, &endr, sizeof(FCGI_EndRequestBody));

            if (ret == -1 || ret != sizeof(FCGI_EndRequestBody)) {
                free(conBuf);
                free(errBuf);
                return -1;
            }

            //把内容发送到客户端
            char buf[MAXLINE];
            sprintf(buf, "HTTP/1.1 200 OK\r\n");
            sprintf(buf, "%sServer: hz_Server\r\n", buf);
            sprintf(buf, "%sContent-Length: %d\r\n", buf, outlen + errlen);
            sprintf(buf, "%sContent-Type: %s\r\n\r\n", buf, "text/html");
            if (rio_writen(fd, buf, strlen(buf)) < 0) {
                hz_error("php error", "write to client failed", log_file);
            }

            if (outlen > 0) {
                if (rio_writen(fd, conBuf,  outlen)< 0) {
                    return -1;
                }
            }

            if (errlen > 0) {
                if (rio_writen(fd, errBuf, errlen) < 0) {
                    return -1;
                }
            }
            //
            free(conBuf);
            free(errBuf);
            return 0;
        }
    }
    return 0;
}

