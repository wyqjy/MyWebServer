
项目描述：使用IO多路复用的EPOLL技术，该服务器并发处理客户端对静态资源的请求，并在服务端使用多线程技术加快响应速度

主要工作：
    1. 基于IO复用技术Epoll和线程池实现多线程的Reatcor高并发服务器；
    2. 利用有限状态机解析HTTP请求报文，实现处理静态资源的请求；
    3. 利用RAII机制实现了数据库连接池，实现了用户注册登录功能
    4. 实现了定时器，关闭超时的不活跃连接
    5. 利用单例模式与阻塞队列实现异步日志系统，记录服务器运行状态

使用redis做缓存
./w1 -c 500 -t 30 http://43.143.195.140:9999/welcome.html
Webbench - Simple Web Benchmark 1.5
Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.

Benchmarking: GET http://43.143.195.140:9999/welcome.html
500 clients, running 30 sec.

Speed=62624 pages/min, 117002 bytes/sec.
Requests: 31308 susceed, 4 failed.



压力测试：经过Webbench压力测试，可以实现上万的并发连接数据交换

