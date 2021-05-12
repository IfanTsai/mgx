[中文](./README_CN.md)

## **A high performance network framework using epoll, connections pool and threads pool (support tcp and http)**

The framework doesn't depend on any third party components

### The framework mainly realizes the following points

- a master process and multiple subprocesses(worker processes)
    - the master process is responsible for signal processing
        - such as reviving the subprocess, log rotate (`kill -USER1 master_pid`) ...
    - the worker processes handle specific network requests (use epoll LT mode)
- configuration file and log system
- threads pool processing client messages
- a thread and message queues handle the sending of messages
- delay reclaiming connections in connection pool
- heartbeat packet mechanism and detection timer queue to reclain connections
- using specific message format to solve tcp packet sticking problem



By default, the source code is configured to **http** mode. If you need to configure tcp mode, please modify the variable of **config.mk** under the root path.

```
export USE_HTTP = false
```

- Implementation of tcp mode in subprocess [Mgx_logic_socket](./bussiness/mgx_logic_socket.cpp)
- Implementation of http mode in subprocess [Mgx_http_socket](./http/mgx_http_socket.cpp)



In addition, this project also implements a coroutine library, and implements a set of cosockets based on this coroutine library, but note that it's not integrated into above framework. You can see their use in the test directory.

Mainly realize code:

- [coroutine](./misc/mgx_coroutine.cpp)
- [coroutine schedule](./misc/mgx_coroutine_scheduler.cpp)
- [cosocket](./misc/mgx_cosocket.cpp)


### The general structure of Mgx
![](https://img.caiyifan.cn/mgx_structure_new.jpg)
