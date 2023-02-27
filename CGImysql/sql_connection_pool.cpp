//
// Created by Yuqi on 2023/2/27.
//

#include "sql_connection_pool.h"
using namespace std;

connection_pool::connection_pool() {
    m_CurConn = 0;
    m_FreeConn = 0;
}

connection_pool *connection_pool::GetInstance() {
    static connection_pool connPool;
    return &connPool;
}

void connection_pool::init(string url, string User, string Password, string DataBaseName, int Port, int MaxConn,
                           int close_log) {

    m_url = url;
    m_User = User;
    m_Port = Port;
    m_password = Password;
    m_DatabaseName = DataBaseName;
    m_close_log = close_log;

    for(int i=0; i<MaxConn; i++) {
        MYSQL *con = NULL;
        con = mysql_init(con);
        if(con == NULL) {

            exit(1);
        }
        con = mysql_real_connect(con, m_url.c_str(), m_User.c_str(), m_password.c_str(), m_DatabaseName.c_str(), m_Port, NULL, 0);
        if(con == NULL){
            exit(1);
        }
        connList.push_back(con);
        ++m_FreeConn;
    }

    reserve = sem(m_FreeConn);
    m_MaxConn = m_FreeConn;
}

// 当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *connection_pool::GetConnection() {
    MYSQL *con = NULL;

    if(connList.size() == 0)
        return NULL;

    reserve.wait();
    lock.lock();

    con = connList.front();
    connList.pop_front();

    --m_FreeConn;
    ++m_CurConn;

    lock.unlock();
    return con;
}

// 释放当前使用的连接
bool connection_pool::ReleaseConnection(MYSQL *con) {
    if(con == NULL) {
        return false;
    }

    lock.lock();
    connList.push_back(con);

    ++m_FreeConn;
    --m_CurConn;

    lock.unlock();
    reserve.post();
    return true;
}

// 销毁数据库连接池
void connection_pool::DestroyPool() {

    lock.lock();
    if( connList.size()> 0 ){
        list<MYSQL*>::iterator it;
        for(it = connList.begin(); it!=connList.end(); ++it) {
            MYSQL *con = *it;
            mysql_close(con);
        }
        m_CurConn = 0;
        m_FreeConn = 0;
        connList.clear();
    }
    lock.unlock();
}

int connection_pool::GetFreeConn() {
    return this->m_FreeConn;
}

connection_pool::~connection_pool() {
    DestroyPool();
}

connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool) {
    *SQL = connPool->GetConnection();
    conRAII = *SQL;
    pollRAII = connPool;
}

connectionRAII::~connectionRAII() {
    pollRAII->ReleaseConnection(conRAII);
}

