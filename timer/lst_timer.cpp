//
// Created by Yuqi on 2023/2/18.
//

#include "lst_timer.h"
#include "../http/http_conn.h"



sort_timer_lst::sort_timer_lst() {
    head = NULL;
    tail = NULL;
}
sort_timer_lst::~sort_timer_lst() {
    util_timer *tmp = head;
    while(tmp) {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}


// 将定时器结点插入到 lst_head之后合适的位置  timer是升序的
void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head) {

    util_timer *prev = lst_head;
    util_timer *tmp = prev->next;

    while(tmp) {
        if(timer->expire < tmp->expire) {
            timer->prev = prev;
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            break;
        }
        prev = tmp;
        tmp = tmp -> next;
    }

    if(!tmp) {     // 插入到最后
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

void sort_timer_lst::add_timer(util_timer *timer) {    //更新了一个结点的时间戳,把这个结点的位置转到合适的位置，由于链表是升序的，更新时间戳一定是时间更大了，所以就往后找就行
    if(!timer){
        return;
    }
    if(!head){
        head = timer;
        tail = timer;
    }
    if(timer->expire < head->expire) {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return ;
    }
    add_timer(timer, head);
}

void sort_timer_lst::adjust_timer(util_timer *timer) {
    if(!timer){
        return;
    }
    util_timer *tmp = timer->next;
    if(!tmp || (timer->expire < tmp->expire)) {   // 已经就是合适的位置了
        return ;
    }
    if (timer == head) {    // 把timer结点摘出来，之后放到合适的位置
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    }
    else {
        timer->next->prev = timer->prev;
        timer->prev->next = timer->next;
        add_timer(timer, timer->next);
    }
}

void sort_timer_lst::del_timer(util_timer *timer) {    // 从链表中删除这个定时器结点
    if(!timer){
        return ;
    }

    if (timer == head && timer == tail) {
        delete timer;
        head = NULL;
        tail = NULL;
        return ;
    }

    if(timer == head){
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    if(timer == tail) {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

void sort_timer_lst::tick() {     // 定时处理的函数       时间到了，删除结点   但为什么不在这里调用del_timer呢

    printf("时间到了，删除： \n");
    if(!head){
        return;
    }

    time_t cur = time(NULL);
    util_timer *tmp = head;
    while(tmp) {
        if(cur < tmp->expire)  // 当前结点的终止时间已经过了，要把这个结点给删掉
            break;

        tmp->cb_func(tmp->user_data);
        head = tmp->next;
        if(head) {
            head->prev = NULL;
        }
        printf("delete connfd: %d\n", tmp->user_data->sockfd);
        delete tmp;
        tmp = head;
    }
}



void Utils::init(int timeslot) {
    m_TIMESLOT = timeslot;
}

// 将文件描述符设置为非阻塞，返回原先的文件描述符的属性
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

// 将事件重置为EPOLLONESHOT
void Utils::modfd(int epollfd, int fd, int ev, int TRIGMode) {
    epoll_event event;
    event.data.fd = fd;

    if(TRIGMode == 1)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}


void Utils::sig_handler(int sig) {
    //为保证函数的可重入性，保留原来的errno, 这是原来主程序的errno,之后进行定时器处理，可能会改变这个errno,等处理结束后，再变回去
    //可重入性表示中断后再次进入该函数，环境变量与之前相同，不会丢失数据
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

// 设置信号函数
void Utils::addsig(int sig, void (*handler)(int), bool restart) {

    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;     // 回调函数
    if( restart ){                  //使被信号打断的系统调用自动重新发起
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);    // 将信号集中所有标志位都置为1，表示阻塞这个信号
    assert(sigaction(sig, &sa, NULL) != -1);    // 注册捕捉函数sigaction

}


//定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler()
{
    m_timer_lst.tick();
    alarm(m_TIMESLOT);          // 重新设置alarm， 为什么不用setitimer
}

void Utils::show_error(int connfd, const char *info) {
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int* Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;





//class Utils;
void cb_func(client_data *user_data) {
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}
