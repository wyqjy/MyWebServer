//
// Created by Yuqi on 2023/2/18.
//

#include "webserver.h"

WebServer::WebServer() {

    // 创建http_conn对象 每个对象以连接的fd作为下标，保存对应信息
    users = new http_conn[MAX_FD];

    //root文件夹路径
//    char server_path[200];
//    getcwd(server_path, 200);
//    char root[6] = "/root";
//    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
//    strcpy(m_root, server_path);
//    strcat(m_root, root);

    m_root = "../root";

    // 定时器
    users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer() {
    close(m_epollfd);
    close(m_listenfd);

    close(m_pipefd[0]);
    close(m_pipefd[1]);

    delete [] users;
    delete [] users_timer;
    delete m_pool;
}

void WebServer::init(string user, string password, string databaseName, int sql_num, int close_log, int port, int thread_num,
                     int trigmode, int actor_model) {

    m_user = user;
    m_passeord = password;
    m_databaseName = databaseName;
    m_sql_num = sql_num;

    m_port = port;
    m_thread_num = thread_num;
    m_trigmode = trigmode;
    trig_mode();
    m_actormodel = actor_model;

    m_close_log = close_log;
}

void WebServer::trig_mode() {
    switch (m_trigmode) {
        case 0:{            // LT + LT
            m_LISTENTrigmode = 0;
            m_CONNTrigmode = 0;
            break;
        }
        case 1:{            // LT + ET
            m_LISTENTrigmode = 0;
            m_CONNTrigmode = 1;
            break;
        }
        case 2:{            // ET + LT
            m_LISTENTrigmode = 1;
            m_CONNTrigmode = 0;
            break;
        }
        case 3:{            // ET + ET
            m_LISTENTrigmode = 1;
            m_CONNTrigmode = 1;
            break;
        }
        default:{           // 默认是 LT + LT
            m_LISTENTrigmode = 0;
            m_CONNTrigmode = 0;
        }
    }
}

void WebServer::thread_pool() {
    m_pool = new threadpool<http_conn>(m_actormodel, m_connPool, m_thread_num);
}

void WebServer::sql_pool() {
    // 初始化数据库连接池
    m_connPool = connection_pool::GetInstance();
    m_connPool->init("43.143.195.140", m_user, m_passeord, m_databaseName, 3306, m_sql_num,  m_close_log);

    users->initmysql_result(m_connPool);
}

void WebServer::eventListen() {    // 创建监听socket并监听  和创建epoll，

    // 创建监听socket
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    // 设置server地址
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(m_port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    // 设置端口复用
    int flag=1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    // 绑定
    int ret = bind(m_listenfd, (struct sockaddr*)&address, sizeof(address) );
    assert(ret >= 0);

    // 监听
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    // 创建epoll
    m_epollfd = epoll_create(5);   //参数没有意义
    assert(m_epollfd != -1);

    utils.init(TIMESLOT);
    // 向epoll注册监听socket fd 文件描述符， 注意不能设置成oneshot
    //对于监听的sockfd，最好使用水平触发模式，边缘触发模式会导致高并发情况下，有的客户端会连接不上。如果非要使用边缘触发，网上有的方案是用while来循环accept()。
    utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);

    http_conn::m_epollfd = m_epollfd;

    // 创建一对套接字用于通信，项目中使用管道通信
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    utils.setnonblocking(m_pipefd[1]);     // 设置管道写端非阻塞
    utils.addfd(m_epollfd, m_pipefd[0], false, 0);  // 设置管道读端为ET非阻塞

    utils.addsig(SIGPIPE, SIG_IGN);
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false);

    alarm(TIMESLOT);     // 为什么在这里就加了alarm, 现在还没有客户端连接呢

    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;

//    http_conn::utils = utils;

    std::cout<<"监听socket和epoll创建完成"<<std::endl;
}

void WebServer::eventLoop() {

    bool stop_server = false;
    bool timeout = false;  // 表示现在是否超时了   超时标志

    while(!stop_server) {

        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);

        for(int i=0; i<number; i++) {

            int sockfd = events[i].data.fd;

            if(sockfd == m_listenfd){    // 监听fd有反应   有新地连接进来了
                bool flag = dealclientdata();
                if(!flag)    // 新的客户端连接加入失败， 可能是连接队列users满了
                    continue;
            }
            else if ( events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR) ) {
                // 客户端连接关闭，移除对应的定时器
                util_timer  *timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);           // 这里好像没将users[sockfd] 关闭啊， 就是调用users[sockfd].close_conn()    不用调用，在lst_timer的函数指针就做了，关闭sockfd
                std::cout<<sockfd<<" 客户端关闭连接 "<<std::endl;
            }
            // 信号处理
            else if( sockfd == m_pipefd[0] && (events[i].events & EPOLLIN)) {
                printf("处理信号\n");
                bool flag = dealwithsignal(timeout, stop_server);
                if(flag==false){
                    printf("信号处理出错\n");
                }

            }
            else if( events[i].events & EPOLLIN) {
                // 处理来自客户端的数据
                dealwithread(sockfd);
            }
            else if( events[i].events & EPOLLOUT) {
                // 要向客户端发送数据
                dealwithwrite(sockfd);
            }

        }

//        std::cout<<"处理了"<<number<<"个事件"<<std::endl;

    }
    if(timeout) {
        utils.timer_handler();

        timeout = false;
    }


}


void WebServer::deal_timer(util_timer *timer, int sockfd) {
    timer->cb_func(&users_timer[sockfd]);    // 将connfd从epoll中移除，关闭sockfd, http_conn::m_user_count--, 已经建立的客户连接减1
    if(timer) {
        utils.m_timer_lst.del_timer(timer);   // 将定时器从双链表中移除
    }
}

bool WebServer::dealwithsignal(bool &timeout, bool &stop_server) {
    int ret = 0;
    int sig;
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if(ret == -1) {
        return false;
    }
    else if (ret == 0) {
        return false;
    }
    else {
        for (int i = 0; i < ret; ++i) {
            switch (signals[i]) {
                case SIGALRM: {
                    timeout = true;
                    break;
                }
                case SIGTERM: {
                    stop_server = true;
                    break;
                }
            }
        }
    }
    return true;
}

void WebServer::timer(int connfd, struct sockaddr_in client_address) {

    // 创建定时器 设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    util_timer *timer = new util_timer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;
    utils.m_timer_lst.add_timer(timer);

}

void WebServer::adjust_timer(util_timer *timer) {
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    utils.m_timer_lst.adjust_timer(timer);

}


// 处理新进来的客户端连接   处理监听连接
bool WebServer::dealclientdata() {

    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    if (m_LISTENTrigmode == 0 ) {  // 水平触发
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
        if(connfd < 0) {
            return false;
        }
        if(http_conn::m_user_count >= MAX_FD) {   // 连接数量已满
            utils.show_error(connfd, "Internal server busy");
            return false;
        }
        users[connfd].init(connfd, client_address, m_root, m_CONNTrigmode);
//        users[connfd].utils = utils;    // 这样好像不太好

        timer(connfd, client_address);    // 定时器初始化一个结点

        // 提示输出有一个客户端连接加入了进来
        std::cout<<"有一个客户端连接加入了进来， 通信socketfd: "<<connfd<<std::endl;

    }
    else{   // 监听的边缘触发，暂时不管
        std::cout<<"Warning: 监听的边缘触发暂未实现"<<std::endl;
        exit(-1);

    }
    return true;
}

void WebServer::dealwithread(int sockfd) {

    util_timer *timer = users_timer[sockfd].timer;

    if ( m_actormodel == 0) {      // Proactor   在主线程中读取数据
        if( users[sockfd].read_once() ) {
            // 检测到了读事件，并读取了数据
            m_pool->append(&users[sockfd]);
            if(timer) {
                adjust_timer(timer);
            }
        }
        else {   // 对方断开连接或者出错，要把user中对应的对象销毁
//            users->close_conn(true);  // 关闭连接， 如果有定时器的话，应该也可以不管，到时间也会清除，看他的项目里在这就没有，只是清除了定时器，好像不行啊
            deal_timer(timer, sockfd);
        }


    }
    else {                      // Reactor 让子进程读写数据，在线程池的工作函数中需要加以判断

    }

}
void WebServer::dealwithwrite(int sockfd) {
    util_timer *timer = users_timer[sockfd].timer;

    if(m_trigmode == 0) {
        // Proactor
        if(users[sockfd].write()){
//            printf("--- 发送一次\n");

            if(timer){
                adjust_timer(timer);
            }
        }
        else{
//            users[sockfd].close_conn();
            deal_timer(timer, sockfd);
        }
    }
    else {

    }
}