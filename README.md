## **a high performance network framework using epoll LT mode, connections pool and thread pool**

### The framework mainly realizes the following points

- a master process and multiple subprocesses(worker processes)
    - the master process is responsible for signal processing
        - such as reviving the subprocess, log rotate ...
    - the worker processes handle specific network requests

- configuration file and log system

- thread pool processing client messages

- a thread and message queues handle the sending of messages

- delay reclaiming connections in connection pool

- heartbeat packet mechanism and detection timer queue to reclain connections

- using specific message format to solve packet sticking problem

### The general structure of Mgx
![](https://img.caiyifan.cn/mgx_structure_new.jpg)
