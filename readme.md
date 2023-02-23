
实现简易的WebServer

----lock

-----lock.h  线程同步机制封装类


---
遇到的问题：

[-23.02.20]
1. Process finished with exit code 139 (interrupted by signal 11: SIGSEGV)  
    在实现了监听socket和创建了epoll后，通过浏览器访问时，程序终止，并出现了该错误。
    在相应的提示输出中，可以知道新来的连接可以监听到，并可以把新的连接加入到epoll中了

    之后在终端编译了程序，生成core文件， 使用gdb查看了core文件，错误信息如下所示；之后仔细检查了lock.h和threadpool.h，并没有发现问题。
    ![img.png](./readmeIMG/fault1.png)

    最终发现这个低级错误，在主函数里没有去初始化线程池，因此也就没有互斥锁，所以才会提示出在加锁的函数中，没有这个东西； 原先的程序应该就是在请求加入到请求队列时终止的。

把读写数据和http解析都放在一个http_conn类中，导致这个类内容较多，可读性不强，能否将读写数据和http解析分成两个类，之后再用一个类继承他们两，往线程池的请求队列中加入的是这个子类
或者把http的信息封装起来也好点吧



---
[-23.02.22]  
如果成员函数的返回类型是在类内定义的一种新的类型，那么在类外实现该函数的时候，返回类型前需要加上作用域

这里的http解析用的是有限状态机，读取一行解析一行，
C++11那版的应该是先将读取进来的数据，分解成一个map，之后解析的时候，直接去找请求的各种信息，不用一轮一轮的读了。 感觉这种更好一点

在从状态机中，每一行的数据都是以\r\n作为结束字符，空行则仅仅是\r\n。因此，通过查找\r\n将报文拆解成单独的行进行解析。

---
[-23.02.23]   
之前测试的时候，返回的数据有两个，原因是刚设置完EPOLLOUT后就会被立即触发：      
    
新来的一个客户端连接，由listenfd监听到连接请求，有一个数据， 将客户端连接加入到epoll，检测事件若有EPOLLOUT， 则该事件会被立即触发，就又检测到了一次数据
不对，这里没设置EPOLLOUT
不能设置EPOLLOUT的原因在于，EPOLLOUT的触发状态很简单，只要内核缓冲区能写就会触发； 若在LT模式下，加入检测的connfd时就加入事件EPOLLOUT，那么当有连接之后 epollwait就一直的被触发EPOLLOUT

而在ET模式下，当内核缓冲区由满变为不满时，就会触发一次EPOLLOUT， 若写缓冲区还没满的话，新来数据也会触发

**写了一篇博客，具体可见博客**


Q：其实有个疑问，没有注册检测EPOLLOUT事件，为什么可以检测到EPOLLOUT呢？  
A：设置了，在webserver的项目中线程池回调函数process的最后，响应数据准备好但是还没写之后，要修改的状态是EPOLLOUT，那什么时候将其重置呢？

请求报文  
 GET /test.html HTTP/1.1
Host: 43.143.195.140:9999
Connection: keep-alive
Upgrade-Insecure-Requests: 1
User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/110.0.0.0 Safari/537.36 Edg/110.0.1587.50
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7
Accept-Encoding: gzip, deflate
Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6
