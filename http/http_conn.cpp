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

void http_conn::close_conn(bool real_close) {

    if(real_close && (m_sockfd != -1) ) {
        printf("closed %d\n", m_sockfd);
        utils.removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}



//循环读取客户数据，直到无数据可读或对方关闭连接
/*
 * m_read_idx 是什么时候重置的呢？？？？？？？？
 * */
bool http_conn::read_once() {
    if(m_read_idx >= READ_BUFFER_SIZE) {
        return false;
    }

    int bytes_read = 0;   // 本次读到的数据大小 字节数

    if (m_TRIGMode == 0) {  // LT 水平触发
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        m_read_idx += bytes_read;

        if(bytes_read <= 0) {
            return false;
        }

        printf("读取到的数据:\n %s\n", m_read_buf);
        return true;
    }
    else {  // ET 边缘触发   非阻塞下边缘触发需要一次性将所有数据都读出
        while(true) {
            bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
            if(bytes_read == -1) {
                if(errno == EAGAIN || errno == EWOULDBLOCK)   // 没有数据了，结束循环，返回后面的true
                    break;
                return false;
            }
            else if(bytes_read == 0) {     // 对方已关闭连接的客户端
                return false;
            }
            m_read_idx += bytes_read;

        }

        printf("读取到的数据:\n %s\n", m_read_buf);
        return true;
    }

}


void http_conn::process() {

    printf("线程池中的线程正在处理数据：该线程号是 %ld\n", pthread_self());
    sleep(1);

}