//
// Created by Yuqi on 2023/2/18.
//

#include "lst_timer.h"
#include "../http/http_conn.h"


//
void Utils::init(int timeslot) {
    m_TIMESLOT = timeslot;
}
//
//// 将文件描述符设置为非阻塞，返回原先的文件描述符的属性
int Utils::setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode) {

    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP;

    if(TRIGMode == 1) {   // 边缘触发
        event.events |= EPOLLET;
    }

    if(one_shot) {
        event.events |= EPOLLONESHOT;
    }

    // 将新来的连接事件插入到epoll
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);

    // 将socket文件描述符设置为非阻塞
    setnonblocking(fd);
}

void Utils::removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

//// 将事件重置为EPOLLONESHOT
void Utils::modfd(int epollfd, int fd, int ev, int TRIGMode) {
    epoll_event event;
    event.data.fd = fd;

    if(TRIGMode == 1)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}


void Utils::sig_handler(int sig) {     // 回调函数      信号处理函数中仅仅通过管道发送信号值，不处理信号对应的逻辑，缩短异步执行时间，减少对主程序的影响。
    //为保证函数的可重入性，保留原来的errno, 这是原来主程序的errno,之后进行定时器处理，可能会改变这个errno,等处理结束后，再变回去
    //可重入性表示中断后再次进入该函数，环境变量与之前相同，不会丢失数据
    int save_errno = errno;
    int msg = sig;      // 为啥呢？ sig本身就是int，参数也不是指针 引用的
    send(u_pipefd[1], (char *)&msg, 1, 0);      // 信号
    errno = save_errno;
}

//// 设置信号函数
void Utils::addsig(int sig, void (*handler)(int), bool restart) {

    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;     // 回调函数
    if( restart ){                  //使被信号打断的系统调用自动重新发起
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);    // 临时阻塞信号集，表示在处理handler函数的过程中，对所有的信号都屏蔽，等handler处理完后，再将信号集中的信号从屏蔽中移除
    assert(sigaction(sig, &sa, NULL) != -1);    // 注册捕捉函数sigaction

}


//定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler()
{
//    m_timer_lst.tick();
    alarm(m_TIMESLOT);          // 重新设置alarm， 为什么不用setitimer
}

void Utils::show_error(int connfd, const char *info) {
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int* Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

////class Utils;
//void cb_func(client_data *user_data) {
//    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
//    assert(user_data);
//    close(user_data->sockfd);
//    http_conn::m_user_count--;
//}



// 对于结点i，向上调整到合适的位置
void HeapTimer::siftup_(size_t i) {   // 调整堆, 对i找到合适的位置，向上调整（新来的结点最开始在队尾）
    assert(i >= 0 && i<heap_.size());
    size_t j = (i-1)/2;     // 因为是下标所以需要先减去一个 1
    while(j >= 0) {
        if(heap_[j]<heap_[i]) break;
        SwapNode_(i, j);
        i = j;
        j = (i-1) / 2;
    }
}

void HeapTimer::SwapNode_(size_t i, size_t j) {
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());
    std::swap(heap_[i], heap_[j]);
    ref_[heap_[i].id] = i;          // 重新确定位置下标  现在下标i里存放的是原来j的内容
    ref_[heap_[j].id] = j;
}

bool HeapTimer::siftdown_(size_t index, size_t n) {
    assert(index >= 0 && index < heap_.size());
    assert(n >= 0 && n <= heap_.size());
    size_t i = index;
    size_t j = i * 2 + 1;
    while(j < n) {
        if(j + 1 < n && heap_[j + 1] < heap_[j]) j++;  // 是要左右结点选更小的那个
        if(heap_[i] < heap_[j]) break;
        SwapNode_(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > index;
}

void HeapTimer::add(int id, int timeOut, const TimeoutCallBack &cb) {
    assert(id >= 0);
    size_t i;
    if(ref_.count(id) == 0) {
        /* 之前没有这个结点，是个新结点，插入到队尾，调整堆 */
        i = heap_.size();
        ref_[id] = i;
        heap_.push_back({id, Clock::now() + MS(timeOut), cb});
        siftup_(i);
    }
    else {
        // 已经有这个结点了，更新时间，重新调整堆
        i = ref_[id];
        heap_[i].expires = Clock::now() + MS(timeOut);
        heap_[i].cb = cb;
        if(!siftdown_(i, heap_.size())) {  // 更新时间戳，时间变大，就需要向下调整
            siftup_(i);             //若没有向下调整，为什么需要再向上判断呢？ 若不向下则有可能向上，若向下了，则确定了位置了，不用再管
        }
    }

}

void HeapTimer::doWork(int id) {
    // 删除指定id结点，并触发回调函数
    if (heap_.empty() || ref_.count(id) == 0) {  // 堆为空或者没有这个结点
        return;
    }
    size_t i = ref_[id];
    TimerNode node = heap_[i];
    node.cb();
    del_(i);
}

void HeapTimer::del_(size_t index) {
    // 删除index下标的结点，在ref_里面存放的其实就是id对应的下标
    assert(!heap_.empty() && index>=0 &&index<heap_.size());

    // 将要删除的结点换到队尾，然后调整堆
    size_t i = index;
    size_t n = heap_.size()-1;  // 最后一个结点
    assert(i <= n);
    if(i < n) {
        SwapNode_(i, n);
        if(!siftdown_(i, n)) {  // 调整交换之后的，此时的i位置是原先的最后一个结点
            siftup_(i);
        }
    }
    // 队尾元素删除
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}


void HeapTimer::adjust(int id, int timeout) {
    // 调整指定id的结点
    assert(!heap_.empty() && ref_.count(id)>0);
    heap_[ref_[id]].expires = Clock::now() + MS(timeout);
    siftdown_(ref_[id], heap_.size());

}

void HeapTimer::tick() {
    // 清除超时结点
    if( heap_.empty() ){
        return ;
    }
    while(!heap_.empty()) {
        TimerNode node = heap_.front();
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) {   // 为什么要这么比较呢？用计数，直接判断相减的值不行吗
            break;
        }
        cout<<node.id<<" 到时间终止了"<<endl;
        node.cb();
        pop();
    }
}

void HeapTimer::pop() {    // 小根堆的第一个元素出去
    assert(!heap_.empty());
    del_(0);
}

void HeapTimer::clear() {
    ref_.clear();
    heap_.clear();
}

int HeapTimer::GetNextTick() {    //返回的是还有多长时间到达最小结点的终止时间
    tick();    // 清除超时结点
    size_t res = -1;
    if(!heap_.empty()) {   //判断下一个结点是否还是超时的
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if (res < 0) {
            res = 0;
        }
    }
    return res;
}