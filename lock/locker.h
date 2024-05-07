#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

// 遵循RAII————获取资源即进行初始化

// 信号量类   目的是实现进程同步，控制进程顺序
class sem {
public:
    // 无参构造
    sem()
    {
        // sem:一个指向信号量的指针，pshared为0代表为进程内信号量，不为0代表进程间通信，value代表初始的信号量的值
        if (sem_init(&m_sem, 0, 0) != 0) {
            throw std::exception();
        }
    }
    // 定义初始value
    sem(int num)
    {
        if (sem_init(&m_sem, 0, num) != 0) {
            throw std::exception();
        }
    }
    // 析构释放信号量资源
    ~sem()
    {
        sem_destroy(&m_sem);
    }
    // 进行原子操作减少信号量的值
    bool wait()
    {
        return sem_wait(&m_sem) == 0;
    }
    // 进行原子操作增加信号量的值
    bool post()
    {
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem;
};

// 互斥锁，目的是使多线程在同一刻只有一个线程对共享资源进行操作。
class locker {
public:
    locker()
    {
        // m:指向pthread_mutex_t类型的指针，它指向将要初始化的互斥锁,
        // a:指向pthread_mutexattr_t类型的指针，用于指定互斥锁的类型,填入NULL则使用默认的属性进行初始化.
        if (pthread_mutex_init(&m_mutex, NULL) != 0) {
            throw std::exception();
        }
    }
    // 析构函数销毁申请的pthread_mutex_t的对象资源
    ~locker()
    {
        pthread_mutex_destroy(&m_mutex);
    }
    // 对pthread_mutex_对象进行加锁
    bool lock()
    {
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    // 对pthread_mutex_t对象进行解锁
    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }
    // 获取该pthread_mutex_t对象的地址
    pthread_mutex_t* get()
    {
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;
};

// 条件变量，用于同步线程间的操作
class cond {
public:
    cond()
    {
        if (pthread_cond_init(&m_cond, NULL) != 0) {
            // pthread_mutex_destroy(&m_mutex);
            throw std::exception();
        }
    }
    ~cond()
    {
        pthread_cond_destroy(&m_cond);
    }
    // 阻塞操作
    bool wait(pthread_mutex_t* m_mutex)
    {
        return pthread_cond_wait(&m_cond, m_mutex)==0;
    }
    // 等候多少秒
    bool timewait(pthread_mutex_t* m_mutex, struct timespec t)
    {
        int ret = 0;
        // pthread_mutex_lock(&m_mutex);
        ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
        // pthread_mutex_unlock(&m_mutex);
        return ret == 0;
    }
    // 给条件变量发送signal信号，若有多个等待该条件变量的线程，则只有一个会被唤醒
    bool signal()
    {
        return pthread_cond_signal(&m_cond) == 0;
    }
    // 给条件变量发送broadcast信号,会唤醒所有等待该条件变量的线程
    bool broadcast()
    {
        return pthread_cond_broadcast(&m_cond) == 0;
    }

private:
    // static pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
};
#endif
