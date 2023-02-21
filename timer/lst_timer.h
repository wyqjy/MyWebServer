//
// Created by Yuqi on 2023/2/18.
//

#ifndef MYWEBSERVER_LST_TIMER_H
#define MYWEBSERVER_LST_TIMER_H


#include <sys/epoll.h>
#include <sys/types.h>
#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>


class Utils {
public:
    Utils() {}
    ~Utils() {}

    void init();

    // 对文件描述符设置非阻塞
    int setnonblocking(int fd);

    // 向epoll中注册事件
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    // 从epoll中移除fd
    void removefd(int epollfd, int fd);

};



#endif //MYWEBSERVER_LST_TIMER_H
