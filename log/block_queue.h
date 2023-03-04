//
// Created by Yuqi on 2023/3/2.
//


/*****
 * 循环数组实现的阻塞队列
 */


#ifndef MYWEBSERVER_BLOCK_QUEUE_H
#define MYWEBSERVER_BLOCK_QUEUE_H

#include <iostream>
#include <cstdio>
#include <pthread.h>
#include <sys/time.h>
#include "../lock/locker.h"

using namespace std;

template<class T>
class block_queue {
public:
    block_queue(int max_size = 1000) {
        if (max_size <= 0) {
            exit(-1) ;
        }

        m_max_size = max_size;
        m_array = new T[max_size];
        m_size = 0;
        m_front = -1;
        m_back = -1;
    }

    void clear() {    // 清空阻塞队列, 应该是重置了，从新开始
        m_mutex.lock();
        m_size = 0;
        m_front = -1;
        m_back = -1;
        m_mutex.unlock();
    }

    ~block_queue() {
        m_mutex.lock();
        if(m_array != NULL) {
            delete [] m_array;
        }
        m_mutex.unlock();
    }

    bool full() {   // 判断队列是否满了
        m_mutex.lock();
        if(m_size >= m_max_size) {
            m_mutex.unlock();
            return  true;
        }
        m_mutex.unlock();
        return false;
    }

    bool empty() {  // 判断队列是否为空
        m_mutex.lock();
        if(m_size == 0){
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    bool front(T &value){   // 通过引用获得队首元素
        m_mutex.lock();
        if(m_size == 0) {
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_front];
        m_mutex.unlock();
        return true;
    }
    bool back(T &value) {   // 返回队尾元素
        m_mutex.lock();
        if( m_size == 0) {
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_back];
        m_mutex.unlock();
        return true;
    }

    int size() {
        int tmp = 0;
        m_mutex.lock();
        tmp = m_size;
        m_mutex.unlock();
        return tmp;
    }

    int max_size() {
        int tmp = 0;
        m_mutex.lock();
        tmp = m_max_size;
        m_mutex.unlock();
        return tmp;
    }

    // 往队列添加元素，然后需要将所有使用队列的线程唤醒
    // 当有元素push进队列，相当于生产者生产了一个元素， 若当前没有线程等待条件变量，则唤醒没有意义,  不是只有一个写线程吗
    bool push(const T &item) {
        m_mutex.lock();
        if(m_size >= m_max_size){
            m_cond.broadcast();
            m_mutex.unlock();
            return false;
        }

        m_back = (m_back+1)%m_max_size;
        m_array[m_back] = item;

        m_size++;

        m_cond.broadcast();
        m_mutex.unlock();
        return true;
    }

    bool pop(T &item) {         //如果当前队列没有元素，将会等待条件变量
        m_mutex.lock();
        while (m_size <= 0) {
            if(!m_cond.wait(m_mutex.get())){   // 等待被唤醒
                m_mutex.unlock();
                return false;
            }
        }

        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;

    }

    bool pop(T &item, int ms_timeout) {     // 增加了超时处理， 但是好像也没用到啊
        struct timespec t = {0,0};
        struct timeval now = {0, 0};
        gettimeofday(&now, NULL);
        m_mutex.lock();
        if(m_size <= 0) {               // 这里怎么不用while了，这个应该是要隔一段时间就要检测一次，而不是一直等
            t.tv_sec = now.tv_sec + ms_timeout / 1000;    // 秒
            t.tv_nsec = (ms_timeout % 1000) * 1000;       // 微秒
            if(!m_cond.timewait(m_mutex.get(), t)) {
                m_mutex.unlock();
                return false;
            }
        }

        if(m_size <= 0) {
            m_mutex.unlock();
            return false;
        }

        m_front = (m_front+1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;

    }


private:
    locker m_mutex;
    cond m_cond;

    T *m_array;
    int m_size;
    int m_max_size;
    int m_front;
    int m_back;

};


#endif //MYWEBSERVER_BLOCK_QUEUE_H
