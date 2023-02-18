//
// Created by Yuqi on 2023/2/18.
//

#ifndef MYWEBSERVER_HTTP_CONN_H
#define MYWEBSERVER_HTTP_CONN_H

class http_conn{
public:
    void process();

public:
    static int m_epollfd;

private:


};


#endif //MYWEBSERVER_HTTP_CONN_H
