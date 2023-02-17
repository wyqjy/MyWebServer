#ifndef MYWEBSERVER_LOCKER_H
#define MYWEBSERVER_LOCKER_H

#include <pthread.h>
#include <semaphore.h>
#include <exception>

// 互斥锁
class locker {
public:
    locker() {
        if( pthread_mutex_init(&m_mutex, NULL) != 0) {   // 初始化互斥锁，成功返回0，失败
            throw std::exception();
        }
    }
    ~locker() {
        pthread_mutex_destroy(&m_mutex);
    }
    bool lock() {
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    bool unlock() {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }
    pthread_mutex_t *get() {
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;
};

class sem {
public:
    sem() {
        if (sem_init(&m_sem, 0, 0) != 0) {
            throw std::exception();
        }
    }
    sem(int num) {
        if(sem_init(&m_sem, 0, num) !=0 ) {
            throw std::exception();
        }
    }
    ~sem() {
        sem_destroy(&m_sem);
    }
    bool wait() {
        return sem_wait(&m_sem) == 0;
    }
    bool post() {
        return sem_post(&m_sem) == 0;
    }
private:
    sem_t m_sem;
};

class cond {
public:
    cond() {
        if(pthread_cond_init(&m_cond, NULL) != 0) {
            throw std::exception();
        }
    }
    ~cond() {
        pthread_cond_destroy(&m_cond);
    }
    bool wait(pthread_mutex_t *mutex) {
        return pthread_cond_wait(&m_cond, mutex) == 0;
    }
    bool timewait(pthread_mutex_t *mutex, struct timespec t){
        return pthread_cond_timedwait(&m_cond, mutex, &t) == 0;
    }
    bool signal() {
        return pthread_cond_signal(&m_cond);
    }
    bool broadcast() {
        return pthread_cond_broadcast(&m_cond);
    }

private:
    pthread_cond_t m_cond;
};



#endif //MYWEBSERVER_LOCKER_H
