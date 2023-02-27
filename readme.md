
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

\r\n的区别：
    \r: 回车， 光标回到旧行（当前行）的开头
    \n: 换行， 另起一新行，光标在新行的开头

---
[-23.02.23]   
之前测试的时候，返回的数据有两个，原因是刚设置完EPOLLOUT后就会被立即触发：      
    
新来的一个客户端连接，由listenfd监听到连接请求，有一个数据， 将客户端连接加入到epoll，检测事件若有EPOLLOUT， 则该事件会被立即触发，就又检测到了一次数据
不对，这里没设置EPOLLOUT
不能设置EPOLLOUT的原因在于，EPOLLOUT的触发状态很简单，只要内核缓冲区能写就会触发； 若在LT模式下，加入检测的connfd时就加入事件EPOLLOUT，那么当有连接之后 epollwait就一直的被触发EPOLLOUT

而在ET模式下，当内核缓冲区由满变为不满时，就会触发一次EPOLLOUT， 若写缓冲区还没满的话，新来数据也会触发

**写了一篇博客，具体可见博客**

当所有数据发送完后，又回设置成EPOLLIN模式，不会检测EPOLLOUT(在write函数中，若数据一次没发送完，会不断触发EPOLLOUT,发送完转为EPOLLIN)

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
\r\n  
  
Q： 对于请求行的解析中，报文里url没有http://XXX.XXX.XXX.XXX什么的，却也需要加以判断吗？  
A： 这会不会是和浏览器有关，有的浏览器发送的请求报文就有这个前缀，所以才会去判断一下

---
  
[-23.02.24]

1. 注意： 解析请求的时候，分解是一个空格加一个\t   " \t"  
   请求头之后的那个空行也在请求头里解析  
  
    请求行的解析会调用一次，只有一行   
    请求头的解析会调用多次，有一行就会调用一次  
    请求体的解析会调用一次(若有的话)，一次就拿到最后  
    

2. char*  和char[] 
        **写了一篇博客**
另写了一个程序，通过char*  直接等于一段字符串，是不能通过指针解引用来改变其中一个字符的，通过键盘录入也不能改，让其等于另一个字符数组却可以。  
相当于让这个字符指针指向了这个数据首地址了
    ```C++
    #include <iostream>
    #include <cstring>
    #include <cstdio>
    using namespace std;
    int main() {
    
        char S[10000] = "GET /test.html HTTP/1.1";
        char* text = 0;
    //    text = "GET /test.html HTTP/1.1";  // 不能通过地址改其中一个字符
        text = S;
    //    printf("%s\n", text);
    //    *(text+3) = 'x';
    //    printf("=== %s\n", text);
    
        char* m_url = strpbrk(text, " \t");
        printf("--- Start%s\n", m_url);
        *m_url++ = '\0';
        printf("通过\n");
    
        char *method = text;
        printf("method: %s\n", method);
        return 0;
    }
    ```

3. Q： m_linger在哪里用到的 保持连接，就是不会close_conn吗？  
    A: 在write()函数中

4.  Q: 请求头的解析中循环条件为什么这样写？
    ```C++
    while((m_check_state==CHECK_STATE_CONTENT && line_status==LINE_OK)||((line_status=parse_line())==LINE_OK))
    ```
      在GET请求报文中，每一行都是\r\n作为结束，所以对报文进行拆解时，仅用从状态机的状态line_status=parse_line())==LINE_OK语句即可。  
    但，在POST请求报文中，消息体的末尾没有任何字符，所以不能使用从状态机的状态，这里转而使用主状态机的状态作为循环入口条件。  

    那后面的&& line_status==LINE_OK又是为什么？  
        解析完消息体后，报文的完整解析就完成了，但此时主状态机的状态还是CHECK_STATE_CONTENT，也就是说，符合循环入口条件，还会再次进入循环，这并不是我们所希望的。  
    为此，增加了该语句，并在完成消息体解析后，将line_status变量更改为LINE_OPEN，此时可以跳出循环，完成报文解析任务。  

5. Q：保存请求体的m_string 字符指针没有初始化 在init()中， 其他的字符指针=0，就是初始化为null吗？  
   A： 指针赋0，就是不指向任何对象，相当于NULL
    
    有些字符指针不初始化会异常中断

6. Q: 请求体的解析，直接到本次消息的m_read_idx(现在读入的数据)就结束，返回GET_REQUEST了， 若一个请求体过长，分成了多个报文传输过来，后面的数据不就没有合到一起吗？  
         这也是因为请求体没有格式，不知道到哪里结束  
        请求体的数据是不是可以这样分开，处理完一个报文，再处理第二个，若是数据的保存和提取的话，可以不连贯到一次操作。而其他需要连贯到一次的操作又一定能在一个报文上传输

7. issue: do_request()中对请求做处理，是按照m_url的第一个字符判断的，并且是用不同的数字表示，那么只要有前面的数字就能访问不同的页面了，而不用名称完全一致  
   A： 这样倒是没啥问题，就是感觉不太好

8. **issue:** 请求页面发现都是NO_RESOURCE 没有资源  
    输出请求的文件 m_real_file查看，路径有问题
    ```
    Real file: /home/ubuntu/MyWebServer/cmake-build-debug-mywebserver/root/picture.html
    ```
   解决办法：  
        a. 使用相对路径  (路径初始化在WebServer的构造函数里，改这里)
        b. 在用getpwd()获取绝对根路径时，对最后一个/cmake-build-debug-mywebserver去掉, 但若有人不用clion，就又不行了, 可以加特判
        c. 在Clion中修改工作路径应该也可以，没试过

9. 可变参数列表 va_list 
    iovec 

---

[-23.02.25]

1. Q: write()函数 向socket写数据的时候，有没有必要在EAGAIN的时候，重新mod(EPOLLOUT)?  
   A:有必要的，哪怕写缓冲区由满变成不满也会触发EPOLLOUT, 但是之前设置了oneshot， 在没有重置之前还是不会触发的。  
    所以有个注意的地方，在设置了oneshot后，如果一次触发处理完后，一定要重置oneshot。

    Q：那就带来一个问题，如果在设置了oneshot后，线程在处理请求时，又来了新的请求，这个请求不会被触发EPOLLIN，那是先放在epoll中吗（等重置后直接触发），不会直接丢弃吧  
        写完后实验一下  
   若eagain则满了，更新iovec结构体的指针和长度，并注册写事件，等待下一次写事件触发（当写缓冲区从不可写变为可写，触发epollout），因此在此期间无法立即接收到同一用户的下一请求，但可以保证连接的完整性（这句指的就是还会放在epoll中，不会直接丢弃吧）。

2. Q：http_conn::write()函数中，对于发送一次后，m_iv的位置更新，原项目有一处不对的地方  
   A：应该是原来的项目不对， 我已经更改过来 

3.  Q: 写完了write函数，但是浏览器端不显示页面？ 
    A: 两个问题，一是写数据的函数应该在Proactor模式下，但是放错在了Reactor下了；二是在http_conn::process_write()中本应该计算bytes_to_send的结果存到了bytes_have_send中了，而使用的bytes_to_send=0,就直接返回发送成功了

    Q：解决了上面的问题后，又发现0号注册页面访问不了？
    A：在http_conn::do_request()函数中strncpy()最后一个参数用了sizeof, 应该用strlen

---
[-23.02.27]
1. Q: 数据库连接池中，销毁数据库连接池，销毁的是list中的连接，有没有可能还有连接在使用还没有回来呢？
   A： 销毁数据库连接池是在析构函数中调用的，此时程序就要结束了，       
        有RAII机制用于自动释放连接（就是将一条连接封装进一个类，新建一个这个对象就是获取这个数据库连接，结束了就调用析构函数释放这个连接）


2. RAII的构造函数第一个参数是二级指针？ 为了获取这个MYSQL指针，所以需要二级指针  
    就是这个第一个参数其实是一个传出参数，需要传出一个数据库连接的指针，所以需要二级指针，否则里面得到的局部变量传不出去
    这就类似于，
    ```C++
    void fun(int b){
        b = 10;
   }
   int a=0;
   fun(a);
   cout<<a<<endl;  // a是0，不是10，若要取得函数变化后的局部结果，需要用指针
   
   void fun1(int *b){
    *b = 10;
   }
   fun1(&a);
   cout<<a<<endl;  //a = 10; 通过函数改变了a的值  在函数里面直接作用于a的地址来改变
   ```
   这里也是类似的，只不过要改变的是一个指针，所以需要二级指针

3. http_conn::mysql好像在使用的时候都是NULL啊  (写完实验一下)
    找到了，这个mysql在线程池那里分配的，而initmysql_result，这个是要得到一个全局变量map，在整个项目运行中，只会调用一次,并且此时还没有创建线程池，就没有连接池资源，所以要自己内部建立connectionRAII  
    同样，在每个线程创立之初就分配一个连接资源，貌似没有体现出连接池的作用了，那为什么不直接在线程里面创建一条数据库连接呢？这不是一样吗？ 一个线程对应一条数据库连接

    在initmysql_result中，初始化的mysql是另一个局部变量吧，函数结束就销毁了  
   initmysql_result这个函数就是把用户名和密码取出来放到一个map里，这个map不是static,因为新的连接可能需要更新
    而在 do_request中用到的mysql呢？    [**我觉得在do_request重新声明connectionRAII mysql会更好，因为有可能用不到，另外对于将用户都拿出来到一个map中，应该到需要的时候再拿，并设置标志表示已经有数据了**]
    
    错了，保存到的不是类里面的m_users,这个没用到，是在http_conn.cpp的上面一个全局变量users
    
    但若是把mysql在这里就从连接池中分配出去了，这个请求并不一定会用到啊


4.  不知道那个关闭日志的有没有用，看每个地方都有，但又都没用到，可能在log里，还没写到呢
5.  I：加完数据库后，运行报错，在初始化数据库那里异常终止？  
    原因：在webserver::init(...)函数中里面的sql_num = sql_num了，使得连接数量值是异常的，后改为m_sql_num好了
