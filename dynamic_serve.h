#ifndef HZ_SERVER_DYNAMIC_SERVE_H
#define HZ_SERVER_DYNAMIC_SERVE_H

#include "http_request.h"

#define FCGI_BEGIN_REQUEST  1   // 请求开始记录类型
#define FCGI_ABORT_REQUEST  2
#define FCGI_END_REQUEST    3   // 响应结束记录类型
#define FCGI_PARAMS         4   // 传输名值对数据
#define FCGI_STDIN          5   // 传输输入数据，例如post数据
#define FCGI_STDOUT         6   // php-fpm响应数据输出
#define FCGI_STDERR         7   // php-fpm错误输出
#define FCGI_DATA           8


/*
 * fastcgi协议报头
 */
struct FCGI_Header{
    unsigned char version;          // 版本
    unsigned char type;             // 协议记录类型
    unsigned char requestIdB1;      // 请求ID
    unsigned char requestIdB0;
    unsigned char contentLengthB1;  // 内容长度
    unsigned char contentLengthB0;
    unsigned char paddingLength;    // 填充字节长度
    unsigned char reserved;         // 保留字节
} ;

/*
 * 请求开始记录的协议体
 */
struct FCGI_BeginRequestBody{
    unsigned char roleB1;   // web服务器期望php-fpm扮演的角色
    unsigned char roleB0;
    unsigned char flags;    // 控制连接响应后是否立即关闭
    unsigned char reserved[5];
} ;



/*
 * 开始请求记录结构，包含开始请求协议头和协议体
 */
struct FCGI_BeginRequestRecord{
    FCGI_Header header;
    FCGI_BeginRequestBody body;
} ;




/*
 * 期望php-fpm扮演的角色值
 */
#define FCGI_RESPONDER  1
#define FCGI_AUTHORIZER 2
#define FCGI_FILTER     3

// 为1，表示php-fpm响应结束不会关闭该请求连接
#define FCGI_KEEP_CONN  1

/*
 * 结束请求记录的协议体
 */
typedef struct FCGI_EndRequestBody{
    unsigned char appStatusB3;
    unsigned char appStatusB2;
    unsigned char appStatusB1;
    unsigned char appStatusB0;
    unsigned char protocolStatus;   // 协议级别的状态码
    unsigned char reserved[3];
} ;

/*
 * 协议级别状态码的值
 */
#define FCGI_REQUEST_COMPLETE   1   // 正常结束
#define FCGI_CANT_MPX_CONN      2   // 拒绝新请求，无法并发处理
#define FCGI_OVERLOADED         3   // 拒绝新请求，资源超负载
#define FCGI_UNKNOWN_ROLE       4   // 不能识别的角色

/*
 * 结束请求记录结构
 */
typedef struct FCGI_EndRequestRecord{
    FCGI_Header header;
    FCGI_EndRequestBody body;
} ;

typedef struct FCGI_ParamsRecord{
    FCGI_Header header;
    unsigned char nameLength;
    unsigned char valueLength;
    unsigned char data[0];
} ;

//动态消息需要用到的信息
struct php_parama
{
    char uri[256];          // 请求地址
    char method[16];        // 请求方法
    char version[16];       // 协议版本
    char filename[256];     // 请求文件名(包含完整路径)
    char name[256];         // 请求文件名(不包含路径，只有文件名)
    char cgiargs[256];      // 查询参数
    char contype[256];      // 请求体类型
    char conlength[16];     // 请求体长度
};



int open_fpm();
void init_php_parama(http_request *r, php_parama *p, char* filename, char* name, char* cigargs);
int send_fastcgi(php_parama* parama, int sock, http_request* p);
int recv_fastcgi(int fd, int sock);
#endif //HZ_SERVER_DYNAMIC_SERVE_H
