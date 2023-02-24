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
    m_user_count++;

    init();


}

void http_conn::init() {
    m_read_idx = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_checked_idx = 0;
    m_start_line = 0;


    m_url = 0;
    m_version = 0;
    m_method = GET;
    cgi = 0;
    m_host = 0;
    m_content_length = 0;
    m_linger = false;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
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
 * m_read_idx 是什么时候重置的呢？  在init中， 这也是为什么init里的内容不能一起放到构造函数里的原因， 处理完一个请求后，需要重置这些读数据和解析的变量
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

    printf("线程池中的线程正在处理数据：该线程号是 %ld\n", pthread_self());


}


// =================================   对读进来  请求的解析
http_conn::HTTP_CODE http_conn::process_read() {

    LINE_STATUS line_status = LINE_OK;   // 读取的一行的状态
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || (line_status = parse_line()) == LINE_OK) {

        text = get_line();              // 获取一行的数据
        m_start_line = m_checked_idx;   // 新的下一行的起始位置

        switch(m_check_state) {
            case CHECK_STATE_REQUESTLINE: {
                ret = parse_request_line(text);
                std::cout<<"ret: "<<ret<<std::endl;
                printf("信息： %s %s\n", m_url, m_version);
                if(ret == BAD_REQUEST)
                    return BAD_REQUEST;
                break;
            }
            case CHECK_STATE_HEADER: {

                break;
            }
            case CHECK_STATE_CONTENT: {
                break;
            }
            default:
                return INTERNAL_ERROR;    // 服务器内部错误，一般不会触发
        }

    }
    return NO_REQUEST;    // 请求不完整，需要继续读取请求报文数据

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



//解析http请求行，获得请求方法，目标url及http版本号       GET /test.html HTTP/1.1
http_conn::HTTP_CODE http_conn::parse_request_line(char *text) {
    m_url = strpbrk(text, " \t");   // 在第一个字符串里找到第一个匹配第二个字符串的一个字符就行   注意分解这里是一个空格加一个\t  就是匹配到空格或者\t都返回
    if(!m_url) {
        return BAD_REQUEST;  // 客户端请求语法错误
    }

    *m_url++ = '\0';
    char *method = text;
    if (strcasecmp(method, "GET") == 0)     // strcasecmp 忽略大小写比较字符串是否相等
        m_method = GET;
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        cgi = 1;
    }
    else
        return BAD_REQUEST;

    m_url += strspn(m_url, " \t");     // 检索字符串 str1 中第一个不在字符串 str2 中出现的字符下标。  这个有必要吗？上面不是已经把\t换成\0， 难道有两个连续的 \t
    m_version = strpbrk(m_url, " \t");
    if (!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';

    m_version += strspn(m_version, " \t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;
    if (strncasecmp(m_url, "http://", 7) == 0)      // 只比较前7个字符     我看读进来的报文直接就是/test.html，前面没有http啥的啊， 需要这样吗？  可能不同浏览器发送的报文不一样吧，有的会带有完整的URL
    {
        m_url += 7;
        m_url = strchr(m_url, '/');                     // 在参数 str 所指向的字符串中搜索第一次出现字符 c（一个无符号字符）的位置
    }

    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;
    //当url为/时，显示判断界面
    if (strlen(m_url) == 1)
        strcat(m_url, "judge.html");

    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;


}