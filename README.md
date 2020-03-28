## **a high performance network framework using epoll LT mode, connections pool and thread pool**

*The framework mainly realizes the following points:*

- a master process and multiple subprocesses
    - the master process is responsible for signal processing
        - such as reviving the subprocess
    - subprocesses handle specific network requests

- configuration file and log system

- thread pool processing client messages

- a thread and message queues handle the sending of messages

- delay reclaiming connections in connection pool

- heartbeat packet mechanism and detection timer queue to reclain connections

- using specific message format to solve packet sticking problem
