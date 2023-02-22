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

    HTTP_CODE read_ret = process_read();  // 解析读进来的请求
    if(read_ret == NO_REQUEST){    // 请求不完整，需要继续读取客户数据
        utils.modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);   // oneshot  重新添加
        return ;
    }

//    printf("线程池中的线程正在处理数据：该线程号是 %ld\n", pthread_self());


}


// =================================   对读进来  请求的解析
http_conn::HTTP_CODE http_conn::process_read() {
    LINE_STATUS line_status = LINE_OK;   // 读取的一行的状态
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || (line_status = parse_line()) == LINE_OK) {

    }




}


//从状态机，用于分析出一行内容
//返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
//    在从状态机中，每一行的数据都是以\r\n作为结束字符，空行则仅仅是\r\n。因此，通过查找\r\n将报文拆解成单独的行进行解析。
http_conn::LINE_STATUS http_conn::parse_line() {
    char temp;
    for ( ; m_checked_idx < m_read_idx; ++m_checked_idx) {
        temp = m_read_buf[m_checked_idx];
        if(temp == '\r') {
            if( (m_checked_idx + 1) == m_read_idx)
                return LINE_OPEN;
            else if (m_read_buf[m_checked_idx+1] == '\n' ) {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp == '\n'){
            if(m_checked_idx > 1 && m_read_buf[m_checked_idx-1]== '\r'){    // 受到传输过来的报文大小和 本地m_read_buf的最大大小限制， 一条数据可能被分成多个报文，尤其是在请求体中
                m_read_buf[m_checked_idx-1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}