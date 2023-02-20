//
// Created by Yuqi on 2023/2/18.
//

#include "lst_timer.h"

void Utils::init() {}

// 将文件描述符设置为非阻塞，返回原先的文件描述符的属性
int Utils::setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode) {

    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP;

    if(TRIGMode == 1) {   // 边缘触发
        event.events |= EPOLLET;
    }

    if(one_shot) {
        event.events |= EPOLLONESHOT;
    }

    // 将新来的连接事件插入到epoll
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);

    // 将socket文件描述符设置为非阻塞
    setnonblocking(fd);
}