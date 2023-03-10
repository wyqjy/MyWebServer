//
// Created by Yuqi on 2023/2/18.
//

#ifndef MYWEBSERVER_LST_TIMER_H
#define MYWEBSERVER_LST_TIMER_H


#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <signal.h>
#include <assert.h>
#include <cstring>

#include <time.h>


class util_timer;

struct client_data {     // 这个和util_timer是你中有我，我中有你，为什么呢？ 难道是一种设计模式吗？
    sockaddr_in address;        //address没用到 不能说这个变量没有用，因为我们可以找到客户端连接的ip地址，用它来做一些业务，比如通过ip来判断是否异地登录等等。
    int sockfd;
    util_timer *timer;    // 感觉这个没啥用啊， 在webserver的dealread和dealwrite中用到
};

class util_timer {
public:
    util_timer(): prev(NULL), next(NULL) {}


    time_t expire;      // 超过这个时间就要断掉这个连接

    void (* cb_func)(client_data *);       // 函数指针， 为什么不能放在类里面呢， 若是只需要一份static不行吗，需要内部数据也是同样的传参数   可能因为sh_handler是函数指针
                                            // 不是，这个是将连接connfd从epoll中移除，关闭connfd,

    client_data *user_data;
    util_timer *prev;
    util_timer *next;
};


class sort_timer_lst {
public:
    sort_timer_lst();
    ~sort_timer_lst();

    void add_timer(util_timer *timer);
    void adjust_timer(util_timer *timer);
    void del_timer(util_timer *timer);
    void tick();

private:
    void add_timer(util_timer *timer, util_timer *lst_head);

    util_timer *head;
    util_timer *tail;
};


class Utils {
public:
    Utils() {}
    ~Utils() {}

    void init(int timeslot);

    // 对文件描述符设置非阻塞
    int setnonblocking(int fd);

    // 向epoll中注册事件
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    // 从epoll中移除fd
    void removefd(int epollfd, int fd);

    void modfd(int epollfd, int fd, int ev, int TRIGMode);


    // 信号处理函数   回调函数
    static void sig_handler(int sig);

    // 设置信号函数
    void addsig(int sig, void(handler)(int), bool restart=true);

    // 定时处理任务，重新定时以不断地触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);

public:
    int m_TIMESLOT;
    static int u_epollfd;
    sort_timer_lst m_timer_lst;

    static int *u_pipefd;

};

void cb_func(client_data *user_data);   // 将要删除的结点的socketfd从epoll中移除

#endif //MYWEBSERVER_LST_TIMER_H
