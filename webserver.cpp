//
// Created by Yuqi on 2023/2/18.
//

#include "webserver.h"

WebServer::WebServer() : timer_(new HeapTimer()){

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
    timeoutMS_ = 60000;
//    users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer() {
    close(m_epollfd);
    close(m_listenfd);

    close(m_pipefd[0]);
    close(m_pipefd[1]);

    delete [] users;
//    delete [] users_timer;
    delete m_pool;
}

void WebServer::close_conn(http_conn *client) {
    assert(client);
    client->close_conn();
    close(client->m_sockfd);
}

void WebServer::init(string user, string password, string databaseName, int sql_num, int close_log, int log_write, int port, int thread_num, int opt_linger,
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
    m_log_write = log_write;
    m_OPT_LINGER = opt_linger;

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

void WebServer::log_write() {
    if(m_close_log == 0) {  // 开启日志
        if(m_log_write == 1){   // 异步
            Log::get_instance()->init("../ServerLog", m_close_log, 2000, 800000, 800);
        }
        else{
            Log::get_instance()->init("../ServerLog", m_close_log, 2000, 800000, 0);
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

    //优雅关闭连接
    if (0 == m_OPT_LINGER)
    {
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if (1 == m_OPT_LINGER)
    {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

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

    utils.addsig(SIGPIPE, SIG_IGN);    // restart = true;
    utils.addsig(SIGALRM, utils.sig_handler, false);   // 时间到了触发
    utils.addsig(SIGTERM, utils.sig_handler, false);   // kill会触发 ctrl+c

    alarm(TIMESLOT);     // 为什么在这里就加了alarm, 现在还没有客户端连接呢

    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;

//    http_conn::utils = utils;

    std::cout<<"监听socket和epoll创建完成"<<std::endl;
}

void WebServer::eventLoop() {

    bool stop_server = false;
    bool timeout = false;  // 表示现在是否超时了   超时标志

    LOG_INFO("Loop Begin");
    while(!stop_server) {

        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }


        for(int i=0; i<number; i++) {

            int sockfd = events[i].data.fd;

            if(sockfd == m_listenfd){    // 监听fd有反应   有新地连接进来了
                bool flag = dealclientdata();
                if(!flag)    // 新的客户端连接加入失败， 可能是连接队列users满了
                    continue;
            }
            else if ( events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR) ) {
                // 客户端连接关闭，移除对应的定时器
                // util_timer  *timer = users_timer[sockfd].timer;
                // deal_timer(timer, sockfd);           // 这里好像没将users[sockfd] 关闭啊， 就是调用users[sockfd].close_conn()    不用调用，在lst_timer的函数指针就做了，关闭sockfd
                close_conn(&users[sockfd]);   //没有对小根堆的结点做删除处理，应该不用管
//                std::cout<<sockfd<<" 客户端关闭连接 "<<std::endl;
            }
            // 信号处理     这个信号将会每个每隔TIMESLOT触发一次， 若没有更新expire时间，会断开三次之前的那个链接
            else if( sockfd == m_pipefd[0] && (events[i].events & EPOLLIN)) {
//                printf("处理信号\n");
                bool flag = dealwithsignal(timeout, stop_server);
                if(flag==false){
                    LOG_ERROR("%s", "dealclientdata failure");
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

        if(timeout) {       // 这个连接收到了SIGALRM信号，时间过去了一个timeslot了
            utils.timer_handler();  // 重置定时器
            int timeMS = timer_->GetNextTick();  //
            LOG_INFO("%s", "timer tick");
//            cout<<"timer tick"<<endl;
            timeout = false;
        }
    }

}


void WebServer::deal_timer(int sockfd) {
//    timer->cb_func(&users_timer[sockfd]);    // 将connfd从epoll中移除，关闭sockfd, http_conn::m_user_count--, 已经建立的客户连接减1
//    if(timer) {
//        utils.m_timer_lst.del_timer(timer);   // 将定时器从双链表中移除
//    }
    close_conn(&users[sockfd]);

    LOG_INFO("close fd %d", sockfd);
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

void WebServer::timer(int connfd) {

    // 创建定时器 设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
//    users_timer[connfd].address = client_address;
//    users_timer[connfd].sockfd = connfd;
//    util_timer *timer = new util_timer;
//    timer->user_data = &users_timer[connfd];
//    timer->cb_func = cb_func;
//    time_t cur = time(NULL);
//    timer->expire = cur + 3 * TIMESLOT;
//    users_timer[connfd].timer = timer;
//    utils.m_timer_lst.add_timer(timer);
    timer_->add(connfd, timeoutMS_,std::bind(&WebServer::close_conn, this, users+connfd));  // 因为调用函数是Webserver的成员函数，且是普通的，其参数就必然会有一个隐含的this指针，现在要通过function来调用它，那么传入的参数也需要this
//    cout<<"将"<<connfd<<" 加入到了定时器堆中"<<endl;
}

void WebServer::adjust_timer(int connfd) {
//    time_t cur = time(NULL);
//    timer->expire = cur + 3 * TIMESLOT;
//    utils.m_timer_lst.adjust_timer(timer);
    if(timeoutMS_ > 0)
        timer_->adjust(connfd, timeoutMS_);
    LOG_INFO("%s", "adjust timer once");

}


// 处理新进来的客户端连接   处理监听连接
bool WebServer::dealclientdata() {

    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    if (m_LISTENTrigmode == 0 ) {  // 水平触发
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
        if(connfd < 0) {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        if(http_conn::m_user_count >= MAX_FD) {   // 连接数量已满
            LOG_ERROR("%s", "Internal server busy");
            utils.show_error(connfd, "Internal server busy");
            return false;
        }
        users[connfd].init(connfd, client_address, m_close_log, m_root, m_CONNTrigmode);
//        users[connfd].utils = utils;    // 这样好像不太好

        timer(connfd);    // 定时器初始化添加一个结点， 定时器中不需要地址信息

        // 提示输出有一个客户端连接加入了进来
//        std::cout<<"有一个客户端连接加入了进来， 通信socketfd: "<<connfd<<std::endl;

    }
    else{   // 监听的边缘触发    没啥用的这个

        while(1) {
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            if(connfd < 0) {
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            if (http_conn::m_user_count >= MAX_FD)
            {
                utils.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            timer(connfd);
        }
        return false;    // 最后为啥返回false,是因为结束的时候一定是出错的时候吗？  并且最后若不断的有新连接进来，while不停，别的连接的请求也无法被处理， 他根本就没把连接加入users,就只管建立连接，其他都不管
    }
    return true;
}

void WebServer::dealwithread(int sockfd) {

//    util_timer *timer = users_timer[sockfd].timer;

    if ( m_actormodel == 0) {      // Proactor   在主线程中读取数据
        if( users[sockfd].read_once() ) {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
            // 检测到了读事件，并读取了数据
            m_pool->append(&users[sockfd], 0);
            if(timeoutMS_ > 0) {
                adjust_timer(sockfd);
            }
        }
        else {   // 对方断开连接或者出错，要把user中对应的对象销毁
//            users->close_conn(true);  // 关闭连接， 如果有定时器的话，应该也可以不管，到时间也会清除，看他的项目里在这就没有，只是清除了定时器，好像不行啊
            deal_timer(sockfd);
        }


    }
    else {                      // Reactor 让子进程读写数据，在线程池的工作函数中需要加以判断
        if(timeoutMS_ > 0) {
            adjust_timer(sockfd);
        }

        m_pool->append(users+sockfd, 0);

        while(true) {   // 等待子线程处理完， improv其实算是一个信号（广义上的），告诉这边我处理完了，之后主线程才能工作，那这样对于主线程不就相当于阻塞的了
            if(users[sockfd].improv == 1) {
                if(users[sockfd].timer_flag == 1) {
                    deal_timer(sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }

}
void WebServer::dealwithwrite(int sockfd) {
//    util_timer *timer = users_timer[sockfd].timer;

    if(m_trigmode == 0) {
        // Proactor
        if(users[sockfd].write()){
//            printf("--- 发送一次\n");
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            if(timeoutMS_ > 0){
                adjust_timer(sockfd);
            }
        }
        else{
//            users[sockfd].close_conn();
            deal_timer(sockfd);
        }
    }
    else {

        if(timeoutMS_ > 0) {
            adjust_timer(sockfd);
        }

        m_pool->append(users+sockfd, 1);

        while(true) {
            if(users[sockfd].improv == 1) {
                if(users[sockfd].timer_flag == 1) {
                    deal_timer(sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }

    }
}