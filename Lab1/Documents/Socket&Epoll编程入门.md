# Socket&Epoll编程入门

## 去哪里找资料

1. 当然最快捷的方法是你可以问助教
2. 当你不知道一个API的使用方法的时候可以查一下man page

以下的所有教程都围绕着TCP协议展开，TCP协议是一个基于连接的可靠传输协议。基于连接是说，两个进程(可以存在于不同的电脑上)要进行通信之前，需要有一个连接的过程。可靠连接是说，我们可以认为除非连接断开，否则两者之间的通信一定会按照发送的次序送达，而不会产生如中间一段数据丢失的情况。

## Socket

关于Socket是什么好像ICS已经讲过了，但是大家恐怕都忘得差不多了，因此这里还是重新讲一下，如果觉得自己已经掌握的很好了，请跳过这个小节

### 一个最简单的例子

这里给出了一个最简单的连接的例子:

**Server**

``` cpp
sock = socket(AF_INET, SOCK_STREAM, 0); // 申请一个TCP的socket
struct sockaddr_in addr; // 描述监听的地址
addr.sin_port = htons(23233); // 在23233端口监听 htons是host to network (short)的简称，表示进行大小端表示法转换，网络中一般使用大端法
addr.sin_family = AF_INET; // 表示使用AF_INET地址族
inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr); // 监听127.0.0.1地址，将字符串表示转化为二进制表示
bind(sock, (struct sockaddr*)&addr, sizeof(addr));
listen(sock, 128);
int client = accept(sock, nullptr, nullptr);
```

**Client**

``` cpp
sock = socket(AF_INET, SOCK_STREAM, 0);
struct sockaddr_in addr;
addr.sin_port = htons(23233);
addr.sin_family = AF_INET;
inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr); // 表示我们要连接到服务器的127.0.0.1:23233
connect(sock, (struct sockaddr*)&addr, sizeof(addr));
```

而传输数据我们只需要使用`recv`和`send`进行即可

**Server**

``` cpp
char buffer[128];
size_t l = recv(client, buffer, 128, 0);
send(client, buffer, l, 0);
```

**Client**

``` cpp
char buffer[128];
sprintf(buffer, "Hello Server");
send(sock, buffer, strlen(buffer)+1, 0);
recv(sock, buffer, 128, 0);
```

这个例子描述了Client向Server发送字符串"Hello Server"，Server将数据发回Client的过程。

但是事实上这样做并不总是正确的，我们可以将send看成是一个缓冲区，这个缓冲区按照固定的速度将内容放到对侧连接的缓冲区中，此时因为已经送达，我们才能将这个缓冲区中已经确认到达的数据清除。调用send的过程等价于我们将要发送的数据拷贝一份放入缓冲区，然后缓冲区会自动发送。但是缓冲区的大小是有限制的，因此调用send并不一定将所有数据都成功放入缓冲区了。

下面给出了一个保证将所有数据放入缓冲区的例子

``` cpp
size_t ret = 0;
while (ret < len) {
    size_t b = send(sock, buffer + ret, len - ret, 0);
    if (b == 0) printf("socket Closed"); // 当连接断开
    if (b < 0) printf("Error ?"); // 这里可能发生了一些意料之外的情况
    ret += b; // 成功将b个byte塞进了缓冲区
}
```

接收端同理:

1. 接收缓冲区是有大小限制的
2. 当我们想接收一定长度的数据的时候，这些数据可能只有部分到达了当前机器

因此我们也需要像这样接收数据才能获取到我们想要的所有信息

## Epoll

考虑以下，当缓冲区为空的时候，我们调用`recv`会发生什么？

会造成线程阻塞，为了解决这个问题我们有几种不同的解决方法:

1. 为每一个socket开一个线程
2. 使用ics课程中教的`select`函数，去轮询所有的socket哪些可读了
3. 使用epoll

**为什么不使用方法1**

在线程间进行调度会引入额外的开销

**为什么不使用方法2**

select的做法实际上是遍历所有需要检查的socket，而这样做相当慢

我们可以认为epoll是一个队列，对于每一个socket我们可以在这个epoll中进行事件的注册，当一个socket触发你监听的事件的时候会将自己加入到这个队列中，我们只需要遍历这个队列即可

> 当然上面这种说法并不准确，epoll内部维护了一个红黑树，和socket有比较复杂的关系，这里为了方便大家理解所以这样说

### 一个简单的例子

**Client**

``` cpp
sock_a ... // sock_a 是一个连接到服务器A的socket
sock_b ... // sock_b 是一个连接到服务器B的socket

int epfd = epoll_create(128); // 参数在新的Linux kernel中无意义
struct epoll_event evt;
evt.events = EPOLLIN;

evt.data.fd = sock_a;
epoll_ctl(epfd, EPOLL_CTL_ADD, sock_a, &evt);

evt.data.fd = sock_b;
epoll_ctl(epfd, EPOLL_CTL_ADD, sock_b, &evt);

struct epoll_event events[16];
while (true) {
    int nevents = epoll_wait(epfd, events, 16, -1);
    for (int i = 0; i < nevents; i ++) {
        int fd = events[i].data.fd;
        if (events[i].events & EPOLLIN) {
            // fd 可以读取了，此时调用recv(fd, ...)不会阻塞
        }
    }
}
```

而对于`epoll`来说，我们常用的也只有`epoll_create, epoll_wait, epoll_ctl`函数

### 常用事件类型

**EPOLLIN**

EPOLLIN事件表示一个socket可以进行读取的事件，对于服务端来说，有两种socket:

1. 监听连接请求的socket
2. 表示对侧的socket

对于第一种socket来说，当调用`accept`可以不阻塞（存在已经创建好的连接），的时候，会触发这个事件

对于第二种socket来说，当这个socket可读的时候，会触发这个事件

**EPOLLOUT**

EPOLLOUT事件表示一个socket可写

**边缘触发和水平触发**

以一个变量为例

`int S = 0`

如果我们希望监听`S = 1`这个事件。水平触发，即当`S = 1`之后每一个时刻我们去检查这个事件都发生；边缘触发，即`S = 1`后，只有第一次我们去检查的时候会触发这个事件。用更形象的语言来描述，边缘触发检测的是变化，水平触发检测的是状态。

EPOLLIN和EPOLLOUT也是同理

以EPOLLIN为例子，当我们的缓冲区中有`"abcdefg"`如果我们调用`recv(sock, buff, 3, 0)`从其中读出了三个字符后，缓冲区中还剩下`"defg"`，如果使用水平触发，那么下一次询问epoll的时候他仍然会返回这个socket存在EPOLLIN事件，如果使用边缘触发，则不会触发。

> Linux 默认采用的水平触发，水平触发更符合直觉，具体为什么可以自己思考一下

### 补充说明

注意到在使用的时候我们使用了`struct epoll_event`

这个结构体定义如下:

``` cpp
struct epoll_event {
  uint32_t events;	/* Epoll events */
  epoll_data_t data;	/* User data variable */
};
```

其中`events`描述了事件的类型

而`data`在使用`EPOLL_CTL_ADD`进行事件注册的时候会被存储，而当你注册的事件发生的时候`data`的内容会返回

这里我只是在里面存储了这个fd的数值类型，事实上你还可以存更多的东西

`epoll_data_t`的定义如下:

``` cpp
typedef union epoll_data {
  void *ptr;
  int fd;
  uint32_t u32;
  uint64_t u64;
} epoll_data_t;
```
