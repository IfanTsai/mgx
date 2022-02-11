[English](./README.md)

## **使用 epoll, 连接池和线程池实现的高性能网络框架 (支持 tcp 和 http)**

该框架不依赖于任何第三方组件

### 主要实现了以下几点

- 一个 master 进程和多个 worker 进程模型
    - master 进程负责信号的处理
        - 比如复活子进程, 日志切割(`kill -USER1 master_pid`) ...
    - worker 进程处理具体网络请求 (使用 epoll 的 LT 模式)
- 配置文件和日志系统
- 线程池处理客户端消息
- 线程通过消息队列处理消息的发送
- 连接池采用延迟回收
- 心跳包机制以及通过检测时间队列来回收无效连接
- 使用特定消息格式来解决 tcp 粘包问题


默认下, 代码配置成了 **http** 模式。 如果需要使用 tcp 模式, 请修改根目录下的 **config.mk** 的变量。

```
export USE_HTTP = false
```

- tcp 模式实现在子类 [Mgx_logic_socket](./bussiness/mgx_logic_socket.cpp)
- http 模式实现在子类 [Mgx_http_socket](./http/mgx_http_socket.cpp)


另外，本项目也实现了一个协程库，并且基于该协程库实现了一套 cosocket 的接口, 但是注意并未整合进代码框架中。你可以在 test 目录下看到他们的用法

主要的实现代码:

- [coroutine](./misc/mgx_coroutine.cpp)
- [coroutine schedule](./misc/mgx_coroutine_scheduler.cpp)
- [cosocket](./misc/mgx_cosocket.cpp)

注：该协程库目前只实现了 x86-64 和 ARM64 平台

### 快速开始

在主机中运行

```bash
make -j4
./mgx
curl 127.0.0.1:8081  # or view web page in browser
```

使用 docker 运行
```bash
docker build -t mgx:latest .
docker run -itd -p 8081:8081 mgx:latest
curl 127.0.0.1:8081  # or view web page in browser
```

你可以在浏览器中访问 http://127.0.0.1:8081.

![image-20220211203019092](https://img.caiyifan.cn/typora_picgo/image-20220211203019092.png) 


### Mgx 的架构简图
![](https://img.caiyifan.cn/mgx_structure_new.jpg)
