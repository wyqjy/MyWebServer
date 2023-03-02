//
// Created by Yuqi on 2023/3/2.
//

#include <cstring>
#include <time.h>

#include "log.h"

Log::Log() {
    m_count = 0;            // 日志的行数初始化为0，那么每次项目运行都是新的（如果需要停服更新呢） 那就都重新开始算呗，这个不是很重要
    m_is_async = false;     // 默认是同步的记录日志
}

Log::~Log() {
    if(m_fp != NULL) {
        fclose(m_fp);
    }
}


// 若max_queue_size 不为0的话，就是要更改模式为异步日志模式
bool Log::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size) {

    // max_queue_size 不为0，设置异步日志模式
//    if (max_queue_size >= 1) {
//        m_is_async = true;
//        m_log_queue = new block_queue<string>(max_queue_size);    // 创建并设置阻塞队列长度
//        pthread_t tid;
//
//        // flush_log_thread是回调函数，创建线程异步写日志
//        int ret = pthread_create(&tid, NULL, flush_log_thread, NULL);
//    }

    m_close_log = close_log;
    m_log_buf_size = log_buf_size;
    m_split_lines = split_lines;

    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size);

    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    const char *p = strrchr(file_name, '/');    // 搜索所指向字符串最后一次出现'/'的位置    日志的名字吗
    char log_full_name[256] = {0};

    if(p == NULL) {
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon+1, my_tm.tm_mday, file_name);
    }
    else {
        //将/的位置向后移动一个位置，然后复制到logname中
        //p - file_name + 1是文件所在路径文件夹的长度
        //dirname相当于./

        strcpy(log_name, p+1);
        strncpy(dir_name, file_name, p-file_name+1);    // 复制路径吗

        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon+1, my_tm.tm_mday, log_name);

    }

    m_today = my_tm.tm_mday;

    m_fp = fopen(log_full_name, "a");
    if(m_fp == NULL) {
        return false;
    }
    return true;

}

void Log::write_log(int level, const char *format, ...) {

    struct timeval now = {0,0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};

    // 日志分级
    switch(level) {
        case 0: {
            strcpy(s, "[debug]:");
            break;
        }
        case 1: {
            strcpy(s, "[info]:");
            break;
        }
        case 2: {
            strcpy(s, "[warn]:");
            break;
        }
        case 3: {
            strcpy(s, "[info]:");
            break;
        }
        default: {
            strcpy(s, "[info]:");
            break;
        }
    }

    // 写一个log,  生成log信息
    m_mutex.lock();
    m_count++;  // 更新现有行数， 但现在还没写呢，是不是放的有点早， 不早，这里要判断需不需要创建新的文件

    // 日志不是今天或写入的日志行数是最大行的倍数    m_split_lines是最大行数（就是一个文件的最大行数 ）   此时需要新建一个文件来保存新的日志
    if( m_today != my_tm.tm_mday || m_count % m_split_lines == 0) {
        char new_log[256] = {0};    // 新创建的文件名（包含路径的）
        fflush(m_fp);   // 刷新缓存区，写到文件中
        fclose(m_fp);   // 关闭原来的文件指针
        char tail[16] = {0};

        // 格式化日志名中的时间部分
        snprintf(tail, 16, "%d_%02d_%02d%_", my_tm.tm_year+1900, my_tm.tm_mon+1, my_tm.tm_mday);

        if( m_today != my_tm.tm_mday) {   // 新的一天
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        else {
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count/m_split_lines);
        }
        m_fp = fopen(new_log, "a");

    }
    m_mutex.unlock();

    // 可变参数
    va_list valst;
    va_start(valst, format);

    string log_str;
    m_mutex.lock();

    //写入内容格式：时间 + 内容
    //时间格式化，snprintf成功返回写字符的总数，其中不包括结尾的null字符
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                    my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                    my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

    //内容格式化，用于向字符串中打印数据、数据格式用户自定义，返回写入到字符数组str中的字符个数(不包含终止符)
    int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, valst);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';

    log_str = m_buf;

    m_mutex.unlock();

    //若m_is_async为true表示异步，默认为同步
    //若异步,则将日志信息加入阻塞队列,同步则加锁向文件中写
//    if (m_is_async && !m_log_queue->full()) {
//        m_log_queue->push(log_str);
//    }
//    else
    {
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }
    va_end(valst);

}

void Log::flush(void) {
    m_mutex.lock();
    //强制刷新写入流缓冲区
    fflush(m_fp);
    m_mutex.unlock();
}












