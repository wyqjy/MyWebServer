//
// Created by Yuqi on 2023/2/20.
//

#include "http_conn.h"

int http_conn::m_epollfd = -1;
int http_conn::m_user_count = 0;
Utils http_conn::utils = Utils();

void http_conn::init(int sockfd, const sockaddr_in &addr, int TRIGMode) {
    m_sockfd = sockfd;
    m_address = addr;
    m_TRIGMode = TRIGMode;

    utils.addfd(m_epollfd, m_sockfd, true, m_TRIGMode);
}

void http_conn::process() {
    printf("线程池中的线程正在处理数据：该线程号是 %ld\n", pthread_self());
    sleep(1);
}