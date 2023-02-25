//
// Created by Yuqi on 2023/2/20.
//

#include "http_conn.h"

//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

int http_conn::m_epollfd = -1;
int http_conn::m_user_count = 0;
Utils http_conn::utils = Utils();

void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode) {
    m_sockfd = sockfd;
    m_address = addr;
    m_TRIGMode = TRIGMode;

    doc_root = root;


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

    m_string = 0;           // 保存请求体数据的，在项目里没有，我自己加的这句
//    m_file_address = 0;     // 在项目里没有，我自己加的这句   unmap()会做

    bytes_to_send = 0;
    bytes_have_send = 0;
    m_write_idx = 0;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
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

//        printf("LT 读取到的数据:\n %s\n", m_read_buf);
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

        // printf("ET 读取到的数据:\n %s\n", m_read_buf);
        return true;
    }

}

// 向socket写数据
bool http_conn::write() {

        int temp = 0;

        if(bytes_to_send == 0) {   // 所有数据都发送出去了，转换检测事件为epollin, 并重新初始化 http_conn这个对象的数据
            utils.modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
            init();
            return true;
        }

        while(1) {
            // 分散写  writev  有多块的内存不是连续的，他就可以一起写进去  (不是多块内存同时写，先写完一块再写另一块)
            temp = writev(m_sockfd, m_iv, m_iv_count);
            if(temp < 0) {

                // 如果TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件，虽然在此期间，
                // 服务器无法立即接收到同一客户的下一个请求，但可以保证连接的完整性。
                if(errno == EAGAIN) {     // 写缓冲满了
                    utils.modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);   // 这句是不有点多余，没有改变这就是默认状态， 直接返回true不行吗？ 还是会触发EPOLLOUT。 【不对， 重点在于oneshot, 不是啊， 由满变为不满还是会触发out的】
                    return true;
                }
                unmap();
                return false;
            }

            bytes_have_send += temp;
            bytes_to_send -= temp;

            // 更新下一次拷贝的内存起始位置
            if (bytes_have_send >= m_iv[0].iov_len) {
                m_iv[0].iov_len = 0;
                m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
                m_iv[1].iov_len = bytes_to_send;
            }
            else {
                m_iv[0].iov_base = m_write_buf + bytes_have_send;
      //          m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;   // 感觉这个不对呢？ bytes_have_send是累加的，这里会把上次减过的再减一次
                m_iv[0].iov_len = m_iv[0].iov_len - temp;         //这个才是这次发送的，   同等表达   m_write_idx - bytes_have_send 或者  bytes_to_send - m_iv[1].iov_len
            }

            if (bytes_to_send <= 0) {
                unmap();
                utils.modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);

                if(m_linger){
                    init();
                    return true;
                }
                else{
                    return false;
                }
            }
        }
}

void http_conn::unmap() {
    if (m_file_address) {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}


void http_conn::process() {

    HTTP_CODE read_ret = process_read();  // 解析读进来的请求
    if(read_ret == NO_REQUEST){    // 请求不完整，需要继续读取客户数据, 其他返回的状态不论是否成功，都要返回响应报文
        utils.modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);   // oneshot  重新添加
        return ;
    }

//    printf("线程池中的线程正在处理数据：该线程号是 %ld\n", pthread_self());

    bool write_ret = process_write(read_ret);
    if (!write_ret) {
        close_conn();
    }
    utils.modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);    //检测EPOLLOUT事件

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
                if(ret == BAD_REQUEST)
                    return BAD_REQUEST;
                break;
            }
            case CHECK_STATE_HEADER: {
                ret = parse_headers(text);
                if(ret == BAD_REQUEST)
                    return BAD_REQUEST;
                else if(ret == GET_REQUEST) {
                    return do_request();
                }
                break;
            }
            case CHECK_STATE_CONTENT: {
                ret = parse_content(text);
                if(ret == GET_REQUEST)
                    return do_request();
                line_status = LINE_OPEN;        // 跳出循环
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
        //如果当前字符是\n，也有可能读取到完整行
        //一般是上次读取到\r就到buffer末尾了，没有接收完整，再次接收时会出现这种情况
        else if(temp == '\n'){
            if(m_checked_idx > 1 && m_read_buf[m_checked_idx-1]== '\r'){    // 受到传输过来的报文大小和 本地m_read_buf的最大大小限制， 一条数据可能被分成多个报文，尤其是在请求体中
                m_read_buf[m_checked_idx-1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    //并没有找到\r\n，需要继续接收
    return LINE_OPEN;
}


//解析http请求行，获得请求方法，目标url及http版本号       GET /test.html HTTP/1.1
http_conn::HTTP_CODE http_conn::parse_request_line(char *text) {

    //在HTTP报文中，请求行用来说明请求类型,要访问的资源以及所使用的HTTP版本，其中各个部分之间通过\t或空格分隔。
    // 请求行中最先含有空格和\t任一字符的位置并返回

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

    //m_url此时跳过了第一个空格或\t字符，但不知道之后是否还有
    //将m_url向后偏移，通过查找，继续跳过空格和\t字符，指向请求资源的第一个字符
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
    //当url为/时，显示欢迎界面
    if (strlen(m_url) == 1)
        strcat(m_url, "judge.html");

    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;


}


//解析http请求头
http_conn::HTTP_CODE http_conn::parse_headers(char *text) {
//  Host: 43.143.195.140:9999
//  Connection: keep-alive
//  Upgrade-Insecure-Requests: 1
//  User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/110.0.0.0 Safari/537.36 Edg/110.0.1587.50
//  Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7
//  Accept-Encoding: gzip, deflate
//  Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6
//  \r\n

    if(text[0] == '\0') {   // 请求头之后有一个空行，判断是不是空行
        if(m_content_length != 0 ){    // 存在请求体，一般post会有请求体
            m_check_state = CHECK_STATE_CONTENT;   // 修改状态，变为解析请求体
            return NO_REQUEST;    // 还需要继续解析
        }
        return GET_REQUEST;    // 解析完一个请求
    }
    else if(strncasecmp(text, "Connection:", 11) == 0) {
        text += 11;
        text += strspn(text, " \t");
        if(strcasecmp(text, "keep-alive") == 0) {
            m_linger = true;   // 保持连接
        }
    }
    else if(strncasecmp(text, "Content-length:", 15) == 0) {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if(strncasecmp(text, "Host:", 5) == 0) {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else{       // 对于其他信息，先跳过，不加以解析
        printf("oop! unknow header: %s\n", text);
    }
    return NO_REQUEST;
}


//解析请求体
http_conn::HTTP_CODE http_conn::parse_content(char *text) {
    if( m_read_idx >= (m_content_length + m_checked_idx) ) {     // 他这是一次性读到 本次read的最后，若一条消息分成两个报文，后面的岂不是就不连贯了
        text[m_content_length] = '\0';
        // POST请求中最后为输入的用户名和密码
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}


// 处理请求     逻辑处理
http_conn::HTTP_CODE http_conn::do_request() {

    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);

//    printf("do m_url: %s\n", m_url);
    const char *p = strchr(m_url, '/');   // 找到m_url中 / 的位置

    // 处理cgi=1（POST） 这里就是登录和注册
    if(cgi == 1 && (*(p+1) == '2' || *(p+1) == '3') ) {

        //根据标志判断是登录检测还是注册检测

        //同步线程登录校验

        //CGI多进程登录校验
    }

    if(*(p + 1) == '0') {     // 请求跳转注册界面
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file+len, m_url_real, strlen(m_url_real));     // 为啥不用strcat呢?   用这个函数的话，如果不是将另一个函数完全复制过去，比如只复制前几个字符，在最后需要手动加 \0

        free(m_url_real);
    }
    else if (*(p + 1) == '1') {   // 跳转到登录界面
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '5') {    // 跳转到图片请求界面
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '6') {    // 跳转到视频请求界面
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '7') {   // 跳转到关注界面
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else {
        //如果以上均不符合，即不是登录和注册，直接将url与网站目录拼接  这里的情况是welcome界面，请求服务器上的一个图片
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
    }


    //通过stat获取请求资源文件信息，成功则将信息更新到m_file_stat结构体, 失败返回-1
    //失败返回NO_RESOURCE状态，表示资源不存在
    if(stat(m_real_file, &m_file_stat)<0)
        return NO_RESOURCE;

    //判断文件的权限，是否可读，不可读则返回FORBIDDEN_REQUEST状态
    if(!(m_file_stat.st_mode&S_IROTH))
        return FORBIDDEN_REQUEST;
    //判断文件类型，如果是目录，则返回BAD_REQUEST，表示请求报文有误
    if(S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;
    //以只读方式获取文件描述符，通过mmap将该文件映射到内存中
    int fd=open(m_real_file,O_RDONLY);
    m_file_address=(char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    //避免文件描述符的浪费和占用
    close(fd);
    //表示请求文件存在，且可以访问
    return FILE_REQUEST;

}


// ================================   生成响应报文
bool http_conn::process_write(HTTP_CODE ret) {

    switch (ret) {
        case INTERNAL_ERROR:{
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            if(!add_content(error_500_form))
                return false;
            break;
        }
        case BAD_REQUEST: {
            add_status_line(404, error_404_title);
            add_headers(strlen(error_404_form));
            if(!add_content(error_404_form))
                return false;
            break;
        }
        case FORBIDDEN_REQUEST: {
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            if(!add_content(error_403_form))
                return false;
            break;
        }
        case FILE_REQUEST: {
            add_status_line(200, ok_200_title);
            if(m_file_stat.st_size != 0) {
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base = m_write_buf;          // 下标是0的这是响应行和响应头
                m_iv[0].iov_len = m_write_idx;
                m_iv[1].iov_base = m_file_address;       // 下标1这是响应体，就是要返回html中的信息
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                bytes_to_send = m_write_idx + m_file_stat.st_size;
                return true;
            }
            else {
                const char *ok_string = "<html><body></body></html>";   // 请求的资源大小为0，返回html空白页
                add_headers(strlen(ok_string));
                if (!add_content(ok_string))
                    return false;
            }
            break;
        }
        default:
            return false;
    }
    // 出错了，就不要响应体了吗， 出错好像也没有响应体； 不对， 还是有响应体的，就是不采用聚集写了
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

bool http_conn::add_response(const char *format, ...) {
    if(m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;               // 可变参数列表
    va_start( arg_list, format);    // 将变量arg_list初始化为传入参数

    int len = vsnprintf(m_write_buf+m_write_idx, WRITE_BUFFER_SIZE-1-m_write_idx, format, arg_list);
    if(len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)) {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);   // 清空可变参数列表

    return true;
}

bool http_conn::add_status_line(int status, const char *title) {
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
bool http_conn::add_headers(int content_length) {
    return add_content_length(content_length) && add_linger() && add_blank_line();
}
bool http_conn::add_content_length(int content_length) {
    return add_response("Content-Length:%d\r\n", content_length);
}
bool http_conn::add_content_type() {
    return add_response("Content-Type:%s\r\n", "text/html");
}
bool http_conn::add_linger() {
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive":"close");
}
bool http_conn::add_blank_line() {
    return add_response("%s", "\r\n");
}
bool http_conn::add_content(const char *content) {
    return add_response("%s", content);
}
