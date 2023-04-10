//
// Created by Yuqi on 2023/2/17.
//

#ifndef MYWEBSERVER_THREADPOOL_H
#define MYWEBSERVER_THREADPOOL_H

#include <pthread.h>
#include <list>
#include <cstdio>

#include <exception>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template<typename T>
class threadpool {
public:
    threadpool(int actor_model, connection_pool *connPool, int thread_number=8, int max_requests=10000);
    ~threadpool();
    bool append(T* request, int state);


private:
    static void* work(void *arg);
    void run();

private:
    int m_thread_number;  // 线程池中的线程数
    int m_max_request;    // 请求队列的最大大小，在等待的请求所允许的最大数量

    pthread_t *m_pthread;   // 线程池数组
    std::list<T*> m_workqueue; //请求队列，线程池就是要抢夺这里面的任务，所以需要互斥锁和信号量(这也是生产者消费者模型)

    locker m_queuelocker;   // 对请求队列的互斥锁
    sem m_queuestat;        // 判断请求队列中是否有任务需要处理  信号量


    connection_pool *m_connPool;  // 数据库连接池
    int m_actor_model;    // 模型切换
};


template<typename T>
threadpool<T>::threadpool(int actor_model, connection_pool *connPool, int thread_number, int max_requests) : m_thread_number(thread_number),
                            m_max_request(max_requests), m_actor_model(actor_model), m_pthread(NULL), m_connPool(connPool)  {
    if( thread_number <= 0 || max_requests <= 0) {
        throw std::exception();
    }

    m_pthread = new pthread_t[m_thread_number];
    if(!m_pthread){
        throw std::exception();
    }
    for(int i=0; i<m_thread_number; i++) {
        int  flag = pthread_create(m_pthread+i, NULL, work, this);
        if( flag != 0) {
            delete [] m_pthread;
            throw std::exception();
        }
        printf("创建了第%d个子线程： %ld\n", i+1, m_pthread[i]);

        if(pthread_detach(m_pthread[i]) != 0){
            delete [] m_pthread;
            throw std::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool() {
    delete[] m_pthread;
}

template<typename T>
bool threadpool<T>::append(T *request, int state) {

    m_queuelocker.lock();
    if(m_workqueue.size() >= m_max_request){
        m_queuelocker.unlock();
        return false;
    }

    request->m_state = state;   // 在这里确定是读还是写
    m_workqueue.push_back(request);
    m_queuestat.post();
    m_queuelocker.unlock();
    return true;
}

template<typename T>
void *threadpool<T>::work(void *arg) {
    threadpool* pool = (threadpool*)arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run() {

    while(true) {
        m_queuestat.wait();
        m_queuelocker.lock();

        if( m_workqueue.empty() ){   // 没有必要，信号量的wait是阻塞的，若wait非阻塞，若队列为空则会不断的wait 信号量不断的-1
            m_queuelocker.unlock();
            continue;
        }

        T* request = m_workqueue.front();
        cout<<request->m_state<<endl;
        m_workqueue.pop_front();
        m_queuelocker.unlock();

        if( m_actor_model == 1){     // Reactor

            if(request->m_state == 0){   // state记录当前需要做的操作是读还是写
                if(request->read_once()) {
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, m_connPool);
                    request->process();
                }
                else{       // 读数据出错了
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else {     // 状态的改变是在本类的append函数
                if(request->write()) {
                    request->improv = 1;
                }
                else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }

        }
        else {                       // Proactor  主线程来处理读写  以同步的方式模拟异步
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            request->process();
        }

    }
}




#endif //MYWEBSERVER_THREADPOOL_H
