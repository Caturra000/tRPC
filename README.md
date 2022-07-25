# tRPC

## TL;DR

一个RPC库，是对我以前造过的轮子[sRPC](https://github.com/Caturra000/sRpc)的改进版（R-S-T，如果有下一版那就是U）

特性如下：

* 协议层是照着[JSON-RPC 2.0 Specification](https://www.jsonrpc.org/specification)写的，但是编译期做了协议层抽象
* 基于我的协程库[co](https://github.com/Caturra000/co)和`json`库[vsjson](https://github.com/Caturra000/vsJSON)实现（已内置），无第三方依赖，且各模块可单独使用
* 不需要学习IDL怎么写，也不需要code generator，只有`C++`
* header only，开箱即用

要求如下：

* 必须是`Linux x86-64`环境
* 必须使用`C++17`及以上的`C++`标准
* 建议使用`g++8`及以上的编译器版本

## 快速使用

### 创建实例

使用工厂方法来创建`trpc::Server`和`trpc::Client`：

* API为`Server::make(Endpoint)`和`Client::make() / Client::make(Endpoint)`
* 返回的是`std::optional<T>`，T为`trpc::Server`或者`trpc::Client`
* 如果有任何原因失败，则返回`std::nullopt`，失败原因可以问`errno`

### 绑定服务

`trpc::Server`可以通过成员函数`bind(std::string method, Functor)`进行绑定服务

`Functor`支持任何可调用对象，既：

* 普通函数
* 函数指针
* 成员函数
* `std::function`
* `std::bind`
* `operator()`
* `lambda`

函数签名的参数建议`by-value`，只要提供`json`构造，都可传入，参数个数不限

### 连接Endpoint

`Endpoint`就是`boost::asio`里面的`endpoint`，这里作为IP和port的封装

`Client`提供两种方式进行连接：

* 使用`Client::make(Endpoint peer)`在构造的同时连接，从构造到连接任一步失败都返回`std::nullopt`
* 使用`Client::connect(Endpoint peer)`在一个已构造的实例中连接，只返回是否连接成功，既`bool`

一些细节上的说明：

* 连接实现在底层使用的是`co::connect`，你需要在**协程**内部使用

* 连接过程已考虑了back-off等待重连，且不影响其它协程的执行

* 如果需要必须连接上对端`peer`，自己用同步的方式写个`while`即可；如果不需要`back-off`重试，那就直接获取fd进行非阻塞`::connect()`

* 失败的连接可以查询错误码`client.error()`，错误码是与`errno`共用的，因此可以用`::strerror()`返回对应的字符串

### 远程调用

`trpc::Client`在连接对端后，可以通过`call<T>(std::string method, Arguments...)`执行远程调用

`method`为上述`server`绑定过了的服务，`Arguments`则是远程传入的参数

如果出现等各种预期外的因素，比如：

* 网络错误
* 服务不存在或参数不匹配
* 服务端无法执行实际例程或者过慢执行
* 协议解析失败

全部返回`std::nullopt`，因为我考虑到大家都不爱看失败的原因，因此返回时就不带错误码了。如果需要，询问`error()`接口

注意：不处理`RPC`以外的`exception`

### 代码示例

TODO 先看`test`文件吧

## 实现上的细节和废话

### 不可靠网络

上一版中`sRPC`只作为计算密集型应用的辅助小部件，因此认为网络是可靠的，几乎不可能失败的，因此`client.call`嗯怼连接就行了

这一版本中由于用途的变更，认为网络是不可靠的，即使传输层仍然使用TCP且为长连接，仍需要考虑：

* 请求不可到达
* 请求延迟到达，但是本端已认为放弃请求，且无下一次请求
* 请求延迟到达，但是本端已认为放弃请求，仍有下一次请求
* 请求延迟到达，但是本端已认为放弃请求，仍有下一次请求，且kernel已积攒大量未处理响应
* 两端大概率的崩溃行为，可能是未进行响应则crash，或者部分响应才crash
* 淦不想写了

解决方案我想了以下几种：

* 改为短连接，任何异常则重连重试。但需要牺牲RTT，以及放弃了TCP保证的消息到达的顺序性，既不同的call可能是out-of-order的形式，不过这个要看具体用法
* 仍然使用单个长连接，维护长连接的状态机，状态转移对了即可，只要可能，尽量维护连接。原则是当状态的不变式没法维护时，你就要抛弃可用性
* 维护长连接池，由于延迟或崩溃引起的未可用连接等待一段时间，把未到达请求和响应耗尽，既TCP中的MSL思路。等待也不是直接等，立刻切换池中的可用连接继续处理远程调用

我目前用的是方法二，既维护单个长连接的状态机，简单地说就是

* 如果你是写失败，除非是写入socket buffer的字节数为0，否则长连接必死

* 如果你是读失败，我会保留到下一次调用时再耗尽，避免延迟分组问题

这是考虑到`RPC`过程中，发起方必须是先写后读，且协程、连接、RPC服务是1:1:1的关系（既不分帧、且各服务请求异步）

因此状态的维护就是每个长连接的每次远程服务调用前都保证内核缓冲中没有积累的字节

（当然你手动换个连接再重试也ok，但是你需要考虑之前尽最大努力交付都不能成功，凭什么你手动重试就能成功？）

一个更加合适的方式是提供所有机制：短连接、长连接、复用池全部给出。但是精力上不允许我这么干。。

### 负载均衡

高情商的说法是利用kernel提供的`REUSEPORT`负载均衡

理由是调研了我（前）司造的RPC轮子，以及各大厂（某蚁金融、某团外卖）因各种理由造的库和框架，大家默认的要么是RR，要么是随机

我认为这并不比`REUSEPORT`高明到哪里去

如果是采用自适应算法——你完全可以在`server`端写个`bind`代理包住各种服务，响应中提供当前`server`的负载信息；同时在`client`也写个`call`代理，再从协议层抽出这些信息，下一次再由上层指定哪个连接去处理即可

这并不需要一个库来干，至少不是必须的

### 超时处理

超时处理是我本来不想面对，但不得不做的事情——它太频繁了，以至于一个`ETIMEDOUT`满足不了

在出现时机方面，超时也算是前面不可靠网络中要考虑的事情，但是又不一定是网络引起的，计算负载过重也可能会出现

在处理决策方面，提供的默认决策总是不满意：超时多久？重试几次？重试后干什么？这和做菜放多少盐一样说不准。。（我不会做菜）

目前我用的策略基本是TCP默认值类似，大概会是5-6次，最长阈值也是RTO_MAX的常数倍，但是这些都可以配置，满足各种离谱的需求

至于超时时机方面，网络延迟是已经妥善处理好的，但是`server`端的问题并没有处理：

* 原因是我查过线程中断目前仍然没有标准库提供的方法，更何况这是非对称式协程，类似嵌套的函数，你要是把中间的一层给断开，有什么后果并不好处理
* 但是用多进程就不一样了，不爽`kill`掉即可（`chromium`喜欢这么干）
* 但问题是我并不考虑多进程，没这么重量级的需要
* 如果需要妥协一点，你做好负载均衡、服务别写太糟糕不就好了吗？如果每次调用都这么慢，怎么做都是救不了的；如果只是偶发地、高峰时才会引起慢处理，这就是负载均衡要做的事情

### 序列化问题

序列化用的是`json`，它的性能并不够好，写的`json`库在设计时是为了好用而不是为了高性能（长得像`nlohmann`），另外我也没有重写`json`库的打算，市面上高性能的轮子很多

一个能想到的好处就是debug足够方便，因为你可以直接在`wireshark`里看到传输的是什么

为了尽量弥补序列化问题，我在接口层上都做了简单的协议层抽象，但是这个是局限于编译期，并不能运行时更换

### 服务发现

没有，DNS自行处理吧

或者简单点使用类似gossip一样的协议不断交换节点间的服务信息就好了

### 语言标准问题

我之前的项目都是尽可能卡在`C++11`或者`C++14`，同时这里其实`C++17`特性用得不多，也就使用了`std::optional`和结构化绑定

其中`std::optional`解决的是不再为了表示`null`而必须用到堆上分配（之前的标准库里有RAII又长得像代理类的是智能指针，都绕不过堆上分配）。但是这个可以写个低版本的兼容类处理<del>，这种事情去`boost`库里抄一个就好了</del>

结构化绑定是完全的内部使用，仅仅是开发者写得舒服，对用户来说是一点收益都没有

也就是说并没啥特别的理由必须要上17的标准，但是项目就是我写的，既然没有不能上的理由，那我就上了

下一次要是能抬到20/23的标准我也尽量试试

## 对比旧轮子sRPC

`sRPC`是基于回调+`promise/futre`的形式来使用的（REAME没更新，看代码实例）

我觉得有些异步的事情不好做，还是同步的形式方便点，因此改成了协程

改写的后果是之前写过的连接状态机、缓存管理、碎片整理、日志调试等一堆小模块都没用了

但是实现比以前简洁了非常多，甚至内部都是可以用同步的形式去写的

有得又有失吧

## 性能测试

据**相当不严谨**的性能测试，server端开启16线程，在我的笔记本（实际物理CPU为8核）上能达到约10万QPS

其中客户端从8线程增长到32线程，协程总数从8增长到2048，单次 request / response 的封装开销约64字节

对比[brpc](https://brpc.apache.org/zh/docs/benchmark/)提供的数据

性能表现高于grpc应该没问题，但是离brpc的25万QPS仍有距离（然而别人有更好的测试机器，且最小的物理线程都有24线程）

TODO 完全相同的测试方式，且需要同一设备

## TODO

virtual network
