//
// Created by Yuqi on 2023/2/18.
//

#ifndef MYWEBSERVER_WEBSERVER_H
#define MYWEBSERVER_WEBSERVER_H

/*
    创建监听socket，并加入到epoll中， 读取写入数据
*/
#include <iostream>
#include <cstdio>
#include <unistd.h>
#include <sys/socket.h>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <cerrno>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <cassert>

#include "./threadpool/threadpool.h"
#include "./http/http_conn.h"
#include "./timer/lst_timer.h"

const int MAX_FD = 65536;              // 最大的文件描述符，创建这么大的http_conn, 代表加入的连接，以connfd(连接的sockfd)作为相应的下标
const int MAX_EVENT_NUMBER = 10000;   // 最大事件数

class WebServer {
public:
    WebServer();
    ~WebServer();

    void init(int port, int thread_num, int trigmode=0, int actor_model=0);

    void thread_pool();
    void trig_mode();

    void eventListen();     // 创建监听socket 创建epoll
    void eventLoop();       // 运行

    bool dealclientdata();   // 处理新进来的客户端连接


private:
    int m_listenfd;     // 监听的文件描述符
    int m_port;         // 端口号
    int m_epollfd;      // epoll文件描述符


    http_conn *users;   // 建立连接的客户端，用connfd作为下标可以找到对应的连接信息

    // 触发模式相关
    int m_trigmode;    // 触发组合模式(0-3表示监听和连接fd的四种触发组合) 根据0-3这4个数对下面监听fd和连接fd的水平和边缘触发进行组合
    int m_LISTENTrigmode;   //监听fd的触发模式
    int m_CONNTrigmode;     //连接fd的触发模式

    // 反应堆模式
    int m_actormodel;   // Reactor 或是 Proactor


    epoll_event events[MAX_EVENT_NUMBER];       // epoll的事件数组 接收epoll事件响应的结果（就是epoll_wait）

    // 线程池
    threadpool<http_conn> *m_pool;
    int m_thread_num;

    //  数据库相关
    connection_pool *m_connPool;

    // 定时器相关， 也包括了向epoll中注册 新连接
    Utils utils;

};





#endif //MYWEBSERVER_WEBSERVER_H
