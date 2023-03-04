//
// Created by Yuqi on 2023/3/2.
//

#ifndef MYWEBSERVER_LOG_H
#define MYWEBSERVER_LOG_H

#include <iostream>
#include <cstdio>
#include <pthread.h>
#include <stdarg.h>
#include "block_queue.h"

using namespace std;

class Log{
public:
    // 单例模式   懒汉
    static Log* get_instance() {
        static Log instance;
        return &instance;
    }

    //可选择的参数有日志文件、是否关闭日志， 日志缓冲区大小、最大行数以及最长日志条队列
    bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);

    // 注意线程的回调函数必须是static
    static void* flush_log_thread(void *args){
        Log::get_instance() -> async_write_log();
    }

    void write_log(int level, const char *format, ...);

    void flush(void);


private:
    Log();
    virtual ~Log();         // 为什么设置成虚函数，也不涉及继承多态啥的

    void *async_write_log() {
        string single_log;
        // 从阻塞队列中取出一个日志string, 写入文件
        while( m_log_queue->pop(single_log) ) {
//            printf("异步写日志，拿到一条信息：");
//            cout<<single_log<<endl;
            m_mutex.lock();                 // 在这加锁有必要吗？   不对，这个不是阻塞队列的锁， 是写数据的锁，防止多条写进来对同一个文件指针操作，写在同一个地方（其实这个不会发生，因为异步里面只有一个子线程）
            fputs(single_log.c_str(), m_fp);    // Log::get_instance()->
            m_mutex.unlock();
            flush();    // 这里是类内普通成员函数，所以可以直接使用m_fp和flush()， 不用获取单例
        }
    }


private:
    char dir_name[128];     // 路径名
    char log_name[128];     // log文件名
    int m_split_lines;      // 日志  最大行数   一个文件里最多写多少行日志
    int m_log_buf_size;     // 日志缓冲区大小
    long long m_count;      // 日志行数记录
    int m_today;            // 按天分类，记录当前是哪一天
    FILE *m_fp;             // 打开log文件的指针
    char *m_buf;            // 要输出的内容
    block_queue<string> *m_log_queue; //阻塞队列
    bool m_is_async;                  //是否同步标志位
    locker m_mutex;
    int m_close_log;                  //关闭日志    0打开1关闭


};

#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}




#endif //MYWEBSERVER_LOG_H
