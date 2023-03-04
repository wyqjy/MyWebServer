//
// Created by Yuqi on 2023/2/18.
//

#ifndef MYWEBSERVER_SQL_CONNECTION_POOL_H
#define MYWEBSERVER_SQL_CONNECTION_POOL_H

#include <cstdio>
#include <list>
#include <mysql/mysql.h>
#include <iostream>
#include <string>
#include <cstring>
#include "../lock/locker.h"
#include "../log/log.h"

#include <pthread.h>
#include <cstdlib>
#include <error.h>


using namespace std;


class connection_pool {
public:
    MYSQL *GetConnection();                     //获取数据库连接
    bool ReleaseConnection(MYSQL *conn);        // 释放连接
    int GetFreeConn();                          //获取当前空闲的连接数量
    void DestroyPool();                         // 销毁所有的连接

    // 单例模式
    static connection_pool* GetInstance();

    void init(string url, string User, string Password, string DataBaseName, int port, int MaxConn, int close_log);

private:
    connection_pool();
    ~connection_pool();

    int m_MaxConn;    // 最大连接数
    int m_CurConn;    // 当前已使用的连接数
    int m_FreeConn;   // 当前空闲的连接数
    locker lock;
    list<MYSQL *> connList;   // 连接池
    sem reserve;

public:
    string m_url;       //主机地址
    int m_Port;
    string m_User;
    string m_password;
    string m_DatabaseName;
    int m_close_log;   // 日志开关

};

class connectionRAII {

public:
    connectionRAII(MYSQL **conn, connection_pool *connPool);
    ~connectionRAII();

private:
    MYSQL *conRAII;                 // 一条连接
    connection_pool *pollRAII;      // 连接池

};




#endif //MYWEBSERVER_SQL_CONNECTION_POOL_H
