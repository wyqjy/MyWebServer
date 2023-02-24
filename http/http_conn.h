//
// Created by Yuqi on 2023/2/18.
//

#ifndef MYWEBSERVER_HTTP_CONN_H
#define MYWEBSERVER_HTTP_CONN_H

#include <iostream>

#include <sys/epoll.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>

#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cstdarg>

#include "../timer/lst_timer.h"

class http_conn{
public:

    static const int FILENAME_LEN = 200;   // 请求的完整的路径url的最大长度
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;

    // 报文的请求方法，目前只会用到GET和POST
    enum METHOD
    {
        GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATH
    };
    /*
        解析客户端请求时，主状态机的状态
        CHECK_STATE_REQUESTLINE:当前正在分析请求行
        CHECK_STATE_HEADER:当前正在分析头部字段
        CHECK_STATE_CONTENT:当前正在解析请求体
    */
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT
    };

    // 报文解析的结果
    /*
        服务器处理HTTP请求的可能结果，报文解析的结果
        NO_REQUEST          :   请求不完整，需要继续读取客户数据
        GET_REQUEST         :   表示获得了一个完成的客户请求
        BAD_REQUEST         :   表示客户请求语法错误
        NO_RESOURCE         :   表示服务器没有资源
        FORBIDDEN_REQUEST   :   表示客户对资源没有足够的访问权限
        FILE_REQUEST        :   文件请求,获取文件成功
        INTERNAL_ERROR      :   表示服务器内部错误
        CLOSED_CONNECTION   :   表示客户端已经关闭连接了
    */
    enum HTTP_CODE
    {
        NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST,
        INTERNAL_ERROR, CLOSED_CONNECTION
    };

    // 从状态机的三种状态，即一行数据的状态
    // 1.读取到一个完整的行 2.行出错 3.行数据尚且不完整
    enum LINE_STATUS
    {
        LINE_OK = 0, LINE_BAD, LINE_OPEN
    };



    http_conn() {}
    ~http_conn() {}

    void init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode);
    void close_conn(bool real_close = true);

    void process();


private:
    void init();

    // ---------------------- 解析数据相关
    CHECK_STATE m_check_state;   // 主状态机当前的所处状态
    int m_checked_idx;           // 当前正在分析的字符在读缓冲区的位置
    int m_start_line;
    char *get_line() {return m_read_buf + m_start_line; };  // 通过parse_line函数就将一段拆成了一行了，将原来的\r\n变成\0\0, 所以从这开始的就是一行的数据，到\0结束了

    HTTP_CODE process_read();     // 对读入的数据进行解析   主状态机
    LINE_STATUS parse_line();     // 获取一行数据   从状态机
    HTTP_CODE parse_request_line(char *text);    // 解析请求行
    HTTP_CODE parse_headers(char *text);         // 解析请求头
    HTTP_CODE parse_content(char *text);         // 解析请求体

    // 解析出来的相关属性
    char m_real_file[FILENAME_LEN];  // 客户请求的目标文件的完整路径，其内容等于 doc_root + m_url, doc_root是网站根目录
    char *m_url;                     // 要找的本地资源的路径，一定以 / 开头
    char *m_version;
    METHOD m_method;
    char *m_host;
    int cgi;                // 是否启用的POST
    int m_content_length;   // http请求消息的总长度
    bool m_linger;          // http请求是否保持连接
    char *m_string;         // 存储请求头的数据

    // ---------------------- 对解析后的数据做逻辑处理
    HTTP_CODE do_request();                // 报文解析完后，对请求处理，看看请求的东西在不在之类的。
    struct stat m_file_stat;               // 客户端请求资源的属性

    char *m_file_address;                  // 客户端请求的文件被mmap到内存中的起始位置

    // ---------------------- 对结果生成响应报文
    bool process_write(HTTP_CODE ret);    // 生成响应报文
    // 下面这些函数将会被process_write()调用 填充HTTP应答
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

    int m_write_idx;                      // 写缓冲区中待发送的字节数, 就是已经放到缓冲区的数据
    struct iovec m_iv[2];                 // 采用writev来执行写操作，所以定义下面两个成员，其中m_iv_count表示被写内存块的数量。
    int m_iv_count;                       // 因为有两个部分，0是 m_write_buf 是状态行和响应头部， 1是 m_file_address是响应体，就是要返回的html的内容

    int bytes_to_send;
    int bytes_have_send;


public:   // 读写数据相关


    bool read_once();   // 一次性读取数据



private:   // 读写数据相关
    char m_read_buf[READ_BUFFER_SIZE];   // 保存读入的数据
    int m_read_idx;                      // 已经读入了多少数据

    char m_write_buf[WRITE_BUFFER_SIZE];   // 写缓冲区




public:    // 基本信息
    static int m_epollfd;     // epollfd
    static int m_user_count;  // 当前已经建立连接的客户端数量

    int m_sockfd;             // 和这个客户端连接的fd
    sockaddr_in m_address;    // 客户端的地址
    int m_TRIGMode;           // 这个连接的触发模式

    char *doc_root;

    static Utils utils;              // 包含定时器和 把socketfd加入到epoll中  这里主要用到在客户端连接进来的时候，要将connfd注册到epoll中


private:


};


#endif //MYWEBSERVER_HTTP_CONN_H
