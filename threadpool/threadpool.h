#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

//线程池
template <typename T>
class threadpool
{
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool(int actor_model, connection_pool *connPool, int thread_number = 8, int max_request = 10000);
    ~threadpool();
    //添加任务到请求队列
    bool append(T *request, int state);
    bool append_p(T *request);

private:
    /*工作线程运行的函数，它不断从请求队列中取出任务并执行之*/
    static void *worker(void *arg);
    void run();

private:
    int m_thread_number;        //线程池中的线程数
    int m_max_requests;         //请求队列的容量
    pthread_t *m_threads;       //描述线程池的数组，其大小为m_thread_number
    std::list<T *> m_workqueue; //请求队列
    locker m_queuelocker;       //保护请求队列的互斥锁
    sem m_queuestat;            //是否有任务需要处理
    connection_pool *m_connPool;  //数据库
    int m_actor_model;          //模型切换
};

//线程池构造函数初始化
template <typename T>
threadpool<T>::threadpool( int actor_model, connection_pool *connPool, int thread_number, int max_requests) 
: m_actor_model(actor_model),m_thread_number(thread_number), m_max_requests(max_requests), m_threads(NULL),m_connPool(connPool)
{
    //线程池的大小或请求队列的大小不合法则抛出错误
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    //一个线程指针指向代表线程池的线程数组
    m_threads = new pthread_t[m_thread_number];
    //如果指针为空则代表创建有误，抛出错误
    if (!m_threads)
        throw std::exception();
    //
    for (int i = 0; i < thread_number; ++i)
    {
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        //将线程设置为分离状态
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}
//线程池析构函数
template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}

template <typename T>
bool threadpool<T>::append(T *request, int state)
{
    //加锁
    m_queuelocker.lock();
    //若超出请求队列的大小则不能再append
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    //设置读写
    request->m_state = state;
    //将请求加入请求队列
    m_workqueue.push_back(request);
    //解锁
    m_queuelocker.unlock();
    //信号量+1
    m_queuestat.post();
    return true;
}
template <typename T>
bool threadpool<T>::append_p(T *request)
{
    //加锁
    m_queuelocker.lock();
    //若超出请求队列的大小则不能再append_p
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    //加入工作队列中
    m_workqueue.push_back(request);
    //解锁
    m_queuelocker.unlock();
    //信号量+1
    m_queuestat.post();
    return true;
}
template <typename T>
void *threadpool<T>::worker(void *arg)
{
    //创建一个指针指向线程池数组中的线程
    threadpool *pool = (threadpool *)arg;
    //调用动态调用线程的run函数
    pool->run();
    return pool;
}
template <typename T>
void threadpool<T>::run()
{
    while (true)
    {
        //请求队列的信号量-1
        m_queuestat.wait();
        //加锁
        m_queuelocker.lock();
        //如果请求队列已经为空了则退出
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        //取出请求队列中的工作任务
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        //解锁
        m_queuelocker.unlock();

        if (!request)
            continue;
        //为1模型时
        if (1 == m_actor_model)
        {
            if (0 == request->m_state)
            {
                if (request->read_once())
                {
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, m_connPool);
                    request->process();
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else
            {
                if (request->write())
                {
                    request->improv = 1;
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else
        {
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            request->process();
        }
    }
}
#endif
