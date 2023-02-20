//
// Created by Yuqi on 2023/2/18.
//

#ifndef MYWEBSERVER_HTTP_CONN_H
#define MYWEBSERVER_HTTP_CONN_H

#include <sys/epoll.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>
#include <unistd.h>

#include "../timer/lst_timer.h"

class http_conn{
public:
    http_conn() {}
    ~http_conn() {}

    void init(int sockfd, const sockaddr_in &addr, int TRIGMode);
    void process();

public:
    static int m_epollfd;     // epollfd
    static int m_user_count;  // 当前已经建立连接的客户端数量

    int m_sockfd;             // 和这个客户端连接的fd
    sockaddr_in m_address;    // 客户端的地址
    int m_TRIGMode;           // 这个连接的触发模式

    static Utils utils;              // 包含定时器和把socketfd加入到epoll中


private:


};


#endif //MYWEBSERVER_HTTP_CONN_H
