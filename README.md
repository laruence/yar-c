#Yar C Framework

## Requirement
- libevent
- msgpack

## Install

````
./configure --with-msgpack=/path-to-msgpack --with-libevent=/path-to-libevent
make 
`````

## Example

you can find a example in example folder

#Manual
# 

# Yar C Framework文档















| **版本** | **变更内容** | **变更时间** | **作者** |
| --- | --- | --- | --- |
| V1.0 | 初始版本 | 2012/11/30 | 惠新宸 |
| V1.1 | 增加打包解包 | 2012/12/02/ | 惠新宸 |
|  |  |  |  |
|  |  |  |  |
|  |  |  |  |



目录



## 概述

 在之前, 我们已经实现了Yar Concurrent RPC framework for PHP, 为PHP提供了一个简单方便的并行化调用RPC的方案.

 在随后的时间里, Yar for PHP已经在主站的PL服务并行化中被应用, 并且经过了近俩个月的线上实际服务验证, 效果已经达到预期.

 在大型Web项目中, PHP和C永远都是最好的搭档, 而我们目前跨语言数据交换并没有一套成型可用的方案.

 虽然开源界类似有Thrift这样的跨语言RPC框架, 但因为它开源, 要兼顾各种平台, 所以实现复杂, 并不适合我们从头使用(无论是学习成本, 或者是后续的维护成本).

 基于此, 我们开发了Yar for C, 目的是提供一个方便, 快速, 高性能的, 遵守Yar RPC协议的C服务开发框架.

 代码地址: [https://svn1.intra.sina.com.cn/weibo/platform/libraries/yar/trunk](https://svn1.intra.sina.com.cn/weibo/platform/libraries/yar/trunk)

## 安装

 Yar依赖于libevent和msgpack, 所以, 首先要确保这俩个库安装成功, 一般我们的服务器都提供了libevent, 所以不需要特别设置.

 Msgpack的代码包在: [https://svn1.intra.sina.com.cn/weibo/platform/libraries/msgpck](https://svn1.intra.sina.com.cn/weibo/platform/libraries/msgpck)

 如果这俩个库已经安装成功, 那么Yar的安装也很简单, 下载好代码以后, 进入目录执行

 ./configure  --with-msgpack=/msgpack的安装路径

 make && make install 

 当Yar成功安装以后(make install), example目录下有一个简单的例子, 会有助于学习基于Yar的开发.

## 服务端编程API

 如果你是要基于Yar for C开发一个C服务, 那么Yar\_Server是你要关心的API, Yar for C(以下简称Yar), 它基于libevent和msgpack, 为开发者提供了daemon, pre-fork, socket manipulation, logging, pack/unpack等作为一个Server常用的功能.

### yar\_server\_init

 int yar\_server\_init(char \*hostname);

 初始化Yar\_Server, 参数为一个字符串的监听地址, 比如对于IPV4来说类似:localhost:8888, 127.0.0.1:8888, 必须包含端口号.

 对于Unix domain socket来说, 类似: /tmp/yar.sock

 如果成功返回1, 失败返回0.

 我们也能看到, 并没有Server实例返回, 也就是说, 一个进程只能存在一个Server实例. 这点要注意.

### yar\_server\_set\_opt

 int yar\_server\_set\_opt(yar\_server\_opt opt, void \*val);

 设置Server的参数, 可选的参数有:

typedef enum \_yar\_server\_opt {

  YAR\_STAND\_ALONE,  //是否单例启动, 一般用作调试的时候, 不会fork worker

  YAR\_READ\_TIMEOUT,  //读取请求的超时值

  YAR\_PARENT\_INIT,   //父进程初始化Hook

YAR\_CHILD\_INIT,    //Woker进程初始化Hook

YAR\_CHILD\_USER,   //Woker运行的用户名

YAR\_CHILD\_GROUP,  //Woker运行的group名

  YAR\_CUSTOM\_DATA,  //回调函数自定义参数

YAR\_MAX\_CHILDREN, //pre-fork的worker数目, 必须在1~128之间,

          //一般设置为和CPU核数相同即可

YAR\_PID\_FILE,    //PID文件的产生路径, 默认为空

YAR\_LOG\_LEVEL,   //LOG的级别, 默认为ALL

  YAR\_LOG\_FILE    //日志文件, 日志文件可以为普通文件, 也可以是ronolog

} yar\_server\_opt;

对于不同的选项, val应该是选项的一级指针, 比如, 设置timeout

 #include "yar.h"

 Int timeout = 5;

 Yar\_server\_setopt(YAR\_READ\_TIMEOUT, &timeout);

 

成功返回1, 失败返回0

#### YAR\_STAND\_ALONE

 是否以单进程启动, 一般用在调试的时候, 因为默认的yar server会以daemon, 并且prefork一些子进程出来, 不利于开发调试. 

#### YAR\_READ\_TIMEOUT, 

请求和处理的超时值, 默认为5s

#### YAR\_PARENT\_INIT & YAR\_CUSTOM\_DATA

Yar server会prefork一些子进程, 这个hook容许我们在yar server prefork完成以后, 对于master进程会做一些初始化的工作, 如果我们设置了这个值, 那么yar在做master进程的初始化工作之后, 也会在master进程调用这个hook, 以方便我们做一些只有master进程需要做的初始化工作.

 这个方法的原型是:

 typedef void (\*yar\_init) (void \*data);

 这里要关心的就data, data是一个void\* 指针, 如果我们通过YAR\_CUSTOM\_DATA设置过一个自定义的数据, 那么这个指针就是我们最初设置的这个数据的指针.

 主要用作在整个yar\_server运行过程中, 传递一些我们自定义的数据.

#### YAR\_CHILD\_INIT

 同上, 不过是woker进程初始化的时候被调用. 函数原型也和parent init一样. 也支持自定义数据.

#### YAR\_CHILD\_USER & YAR\_CHILD\_GROUP

 如果设置了, 那么我们的worker进程就会尝试setuid/setgid到这个用户/组运行.

#### YAR\_MAX\_CHILDREN

 要prefork的子进程数目, 一般设置为和CPU的核数相当即可.

#### YAR\_LOG\_FILE & YAR\_LOG\_LEVEl

 Yar server默认的时候会输出一些日志信息, 但是当yar以正常模式启动的时候, 会以daemon模式运行, 这样一来就无法输出日志信息到stderr/stdout.

 所以, 需要设置一个文件/管道日志输出目的地.

 这个选项支持文件, 或者管道, 对于文件自然没有什么好说, 对于管道的话, 主要结合cronolog来使用.

比如:

 "|/home/huixinchen/local/cronolog/sbin/cronolog ./yar\_server\_%M.log"

就是说, 日志输出到当前目录的yar\_server\_\*.log, \*是当前分钟, 也就是说日志会以分钟做分割.

 而对于log level来说, Yar 分为YAR\_DEBUG, YAR\_NOTICE, YAR\_WARN, YAR\_ERROR 5个级别的日志级别.

 当要输出日志的级别大于log level则输出, 否不输出, 所以这个选项是用作过滤输出的.

### yar\_server\_get\_opt

void \* yar\_server\_get\_opt(yar\_server\_opt opt);

获取某个选项的值

成功返回指针的抽象指针, 失败返回NULL

### yar\_server\_register\_handler

int yar\_server\_register\_handler(yar\_server\_handler \*handlers);

注册一个服务函数, 服务函数的原型是:

typedef void (\*yar\_handler) (yar\_request \*request, yar\_response \*response, void \*data);

而, yar\_server\_handler的定义是:

typedef struct \_yar\_server\_handler {

  char \*name;

  int len;

  yar\_handler handler;

} yar\_server\_handler;

其中name就是RPC调用的时候的方法名, 比如:

yar\_server\_handler example\_handlers[] = {

  {"default", sizeof("default") - 1, yar\_handler\_example},

  {NULL, 0, NULL}

};

那么当客户端的RPC请求default方法的时候, yar\_handler\_example就会被调用, 去处理这个请求. 以PHP客户端为例:

default($args); // yar\_handler\_example会处理该请求

?>

成功返回1, 失败返回0

### yar\_server\_run

int yar\_server\_run();

开始运行Server, 这个调用将会开始pre-fork, listening, accpt, process流程.

除非Server被shutdown, 否则这个函数不会返回.

### yar\_server\_shutdown

void yar\_server\_shutdown();

关闭Server, 这个会停止accpt新请求, worker会在处理完当前请求以后退出

### yar\_server\_destroy

void yar\_server\_destroy();

销毁Server

## 客户端编程API

如果, 你是希望请求一个已有的Yar\_Server, 那么Yar\_Client是你要关心的, 它会请求一个Yar\_Server, 并且得到返回.

### yar\_client\_init

yar\_client \* yar\_client\_init(char \*hostname);

实例化一个Yar\_Client, 参数是目的地址

成功返回Yar\_Client实例:

struct \_yar\_client {

  int fd;

  char \*hostname;

  yar\_client\_call call;

};

一般的, 我们不用关心这个结构体的内容, 只需要关心yar\_client\_call的原型:

typedef yar\_response \* (\*yar\_client\_call)(yar\_client \*client, char \*method, uint num\_args, yar\_packager \*packager[]);

也就是说, 当得到一个Client实例以后, 我们就可以对Server发起调用, 比如我们调用Server的default方法, 并且有2个参数, 那么就类似

Yar\_client \*client = yar\_client\_init("tcp://localhost:2222");

Yar\_response \*response = client->call(client, "default", 2, args);

如果, 失败返回NULL, 比如Server不能连接等.

### yar\_client\_destroy

void yar\_client\_destroy(yar\_client \*client);

调用完成后, 销毁一个Yar\_Client实例

## 参数和返回值

 Yar采用msgpack作为打包协议, 并且为开发者封装了一系列简单的API来实现对数据的打包解包

### 解包的相关API

 观察之前的typedef void (\*yar\_handler) (yar\_request \*request, yar\_response \*response, void \*data),

 我们看到, 当请求到来的时候, 我们Server端注册的处理函数被调用, 其中俩个参数分别为yar\_request, 和yar\_response.

 这个时候, 我们首先要关心的是客户端调用传来了几个参数, 参数分别是什么, 参数的信息保存在request->in里面, 它是个yar\_data 指针.

 在这里我们不用关心yar\_data的结构体的组成是什么, 因为我们只需要调用一系列API就可以得到参数.

 Yar协议规定, 所有的参数都打包在一个数组里面, 所以request->in是一个数组.

 于是, 如果我要检查当前的参数个数, 那么就调用:

yar\_data\_type yar\_unpack\_data\_type(const yar\_data \*data, uint \*size);

其中, yar\_data\_type是一个unum, 可选值是:

typedef enum \_yar\_data\_type {

  YAR\_DATA\_NULL = 1,

  YAR\_DATA\_BOOL,

  YAR\_DATA\_LONG,

  YAR\_DATA\_ULONG,

  YAR\_DATA\_DOUBLE,

  YAR\_DATA\_STRING,

  YAR\_DATA\_MAP,

  YAR\_DATA\_ARRAY

} yar\_data\_type;

对于, string, map, 和array, yar\_unpack\_data\_type的第一个参数将会返回他们的长度或者是元素个数, 比如对于

map {'k' => 'v'},

那么. 返回的size是1.

现在我们就知道怎么检查参数个数了吧, 假设我们的例子只接受3个参数?

uint size = 0;

if (yar\_unpack\_data\_type(request->in, &size) != YAR\_DATA\_ARRAY || size != 3) {  
yar\_response\_set\_error(response, "参数检查失败, 只接受3个参数");

  return ;

}

假设我们现在参数已经检查通过, 假设我们接受2个参数, 分别是俩个整数.

那么就通过如下形式获得相关参数内容:

uint arg[2], dummy;

yar\_data \*tmp;

const yar\_data \*parameters = yar\_request\_get\_parameters(request);

yar\_unpack\_iterator \*it = yar\_unpack\_iterator\_init(parameters); //生成迭代器

int index = 0;

do {

 tmp = yar\_unpack\_iterator\_current(it);

 if (yar\_unpack\_data\_type(tmp, &dummy) != YAR\_DATA\_LOG) {

yar\_response\_set\_error(response, "参数检查失败, 只接受整数");

  return ;

 }

 

 arg[index++] = \*(long \*)( yar\_unpack\_data\_value(tmp));

} while(yar\_unpack\_iterator\_next(it));

这样我们的arg就得到了俩个整数参数.

### 打包的相关API

 当我们获得参数, 并且处理完请求以后, 我们需要返回数据给客户端, 这个时候我们就需要和打包的API打交道了. 他们是:

int yar\_pack\_push\_array(yar\_packager \*packager, uint size);

int yar\_pack\_push\_map(yar\_packager \*packager, uint size);

int yar\_pack\_push\_null(yar\_packager \*packager);

int yar\_pack\_push\_bool(yar\_packager \*packager, int val);

int yar\_pack\_push\_long(yar\_packager \*packager, long num);

int yar\_pack\_push\_ulong(yar\_packager \*packager, ulong num);

int yar\_pack\_push\_double(yar\_packager \*packager, double num);

int yar\_pack\_push\_string(yar\_packager \*packager, char \*str, uint len);

int yar\_pack\_push\_data(yar\_packager \*packager, yar\_data \*data);

int yar\_pack\_push\_packager(yar\_packager \*packager, yar\_packager \*data);

int yar\_pack\_to\_string(yar\_packager \*packager, yar\_payload \*payload);

void yar\_pack\_free(yar\_packager \*packager);

不要看函数很多, 但其实很简单. 打包的时候, 是一唯打包顺序, 什么是一维度顺序呢?

比如, 我们要打包如下的格式:


 a => [  
   b, c

 ]

 d => e

}

那么打包的过程就是:

Yar\_packager \*pk = yar\_pack\_start\_map( 2); //我们是一个2个kv的MAP

yar\_pack\_push\_string(pk, "a", 1); //压入第一个key, a

yar\_pack\_push\_array(pk, 2);  //压入第一个key对应的一个2个元素的array

yar\_push\_string (pk, "b", 1);   //第一个元素

yar\_push\_string (pk, "c", 1);   //第二个元素 此时数组已经填充完毕

yar\_push\_string(pk, "d", 1);   //压入第二个key d

yar\_push\_string(pk, "e", 1);   //压入第二个key对应的e



来看一个实际的例子(在example/server.c)


这个API返回了一个3个kv的map给客户端, 第一个元素是status值是long 0

第二个元素是parameters, 传回了客户端请求的参数

第三个是一个map, 返回了一些随意的值.

最后调用yar\_response\_set\_retval设置好返回值, 然后释放内存.

### 总结

 最后, Yar的代码中包含了一个Server 和一个Client的例子, 在example目录下.


 目前已经实现了PHP端请求C服务, C服务互相请求, 考虑到PHP只能通过HTTP协议提供RPC服务, 所以C请求PHP的, 还暂时没有开放.
