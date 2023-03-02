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
#include "./log/log.h"

const int MAX_FD = 65536;              // 最大的文件描述符，创建这么大的http_conn, 代表加入的连接，以connfd(连接的sockfd)作为相应的下标
const int MAX_EVENT_NUMBER = 10000;   // 最大事件数
const int TIMESLOT = 5;                 // 最小超时单位

class WebServer {
public:
    WebServer();
    ~WebServer();

    void init(string user, string password, string databaseName, int sql_num, int close_log, int log_write, int port,
              int thread_num, int opt_linger, int trigmode=0, int actor_model=0);

    void thread_pool();
    void trig_mode();

    void sql_pool();    // 初始化数据库连接池

    void eventListen();     // 创建监听socket 创建epoll
    void eventLoop();       // 运行

    bool dealclientdata();   // 处理新进来的客户端连接

    void dealwithread(int sockfd);   // 处理读数据， 接受请求
    void dealwithwrite(int sockfd);  // 处理写数据， 发送


    // 定时器相关
    void timer(int connfd, struct sockaddr_in client_address);
    void adjust_timer(util_timer *timer);
    void deal_timer(util_timer *timer, int sockfd);
    bool dealwithsignal(bool& timeout, bool& stop_server);

    // 日志
    void log_write();


private:
    int m_listenfd;     // 监听的文件描述符
    int m_port;         // 端口号
    int m_epollfd;      // epoll文件描述符

    int m_OPT_LINGER;

    http_conn *users;   // 建立连接的客户端，用connfd作为下标可以找到对应的连接信息

    char *m_root;    // 资源路径

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
    string m_user;
    string m_passeord;
    string m_databaseName;
    int m_sql_num;

    // 日志相关
    int m_close_log;
    int m_log_write;   // 日志的模式

    // 定时器相关， 也包括了向epoll中注册 新连接
private:
    client_data *users_timer;    // 这是一个连接的信息，不是结点啊，没有前后指针，还没有链表的对象sort_timer_lst, 链表对象在utils里面
    Utils utils;
public:
    int m_pipefd[2];

};





#endif //MYWEBSERVER_WEBSERVER_H
