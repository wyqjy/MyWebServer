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
}

WebServer::~WebServer() {
    close(m_epollfd);
    close(m_listenfd);

    delete [] users;
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

    utils.init();
    // 向epoll注册监听socket fd 文件描述符， 注意不能设置成oneshot
    //对于监听的sockfd，最好使用水平触发模式，边缘触发模式会导致高并发情况下，有的客户端会连接不上。如果非要使用边缘触发，网上有的方案是用while来循环accept()。
    utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);

    http_conn::m_epollfd = m_epollfd;

    http_conn::utils = utils;

    std::cout<<"监听socket和epoll创建完成"<<std::endl;
}

void WebServer::eventLoop() {

    bool stop_server = false;

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
                // 客户端连接关闭
                std::cout<<" 客户端关闭连接 "<<std::endl;
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
            return false;
        }
        users[connfd].init(connfd, client_address, m_root, m_CONNTrigmode);

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

    if ( m_actormodel == 0) {      // Proactor   在主线程中读取数据
        if( users[sockfd].read_once() ) {
            // 检测到了读事件，并读取了数据
            m_pool->append(&users[sockfd]);
        }
        else {   // 对方断开连接或者出错，要把user中对应的对象销毁
            users->close_conn(true);  // 关闭连接， 如果有定时器的话，应该也可以不管，到时间也会清除，看他的项目里在这就没有，只是清除了定时器，好像不行啊
        }


    }
    else {                      // Reactor 让子进程读写数据，在线程池的工作函数中需要加以判断

    }

}
void WebServer::dealwithwrite(int sockfd) {

    if(m_trigmode == 0) {
        // Proactor
        if(users[sockfd].write()){
            printf("--- 发送一次\n");
        }
        else{
            users[sockfd].close_conn();
        }
    }
    else {

    }
}