
实现简易的WebServer

----lock

-----lock.h  线程同步机制封装类



遇到的问题：

    1. Process finished with exit code 139 (interrupted by signal 11: SIGSEGV)  [-23.02.20]

        在实现了监听socket和创建了epoll后，通过浏览器访问时，程序终止，并出现了该错误。
        在相应的提示输出中，可以知道新来的连接可以监听到，并可以把新的连接加入到epoll中了