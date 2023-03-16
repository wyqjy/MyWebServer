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


#include <queue>
#include <map>
#include <chrono>   // C++11提供的时间库，提供计时，时钟等功能
#include <functional>



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


    static int *u_pipefd;

};

//void cb_func(client_data *user_data);   // 将要删除的结点的socketfd从epoll中移除





// 使用小根堆
typedef std::function<void()> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp;

struct TimerNode{
    int id;
    TimeStamp expires;
    TimeoutCallBack cb;
    bool operator<(const TimerNode& t) {
        return expires < t.expires;
    }
};

class HeapTimer {
public:
    HeapTimer() { heap_.reserve(64); }
    ~HeapTimer() { clear(); }

    void adjust(int id, int newExpires);
    void add(int id, int timeOut, const TimeoutCallBack& cb);
    void doWork(int id);


    void clear();
    void tick();
    void pop();
    int GetNextTick();

private:
    void del_(size_t i);

    void siftup_(size_t i);

    bool siftdown_(size_t index, size_t n);

    void SwapNode_(size_t i, size_t j);

    std::vector<TimerNode> heap_;
    std::unordered_map<int, size_t> ref_;       // ref表示插入的这个int结点时，堆里面的结点数量，也就是他在vector中的下标

};





#endif //MYWEBSERVER_LST_TIMER_H
