## 使用说明
~~备忘录（bushi）~~
- 本次实验用 Socket 不可靠的数据包模式实现类似 TCP 的可靠传输。源端口设为5000，路由器端口设为6000，目的端口设为7000。
- 运行时首先配置路由器的路由IP端口和目的端的IP端口。启动后再先后启动Receiver.exe和Sender.exe，输入文件路径后即可开始传输，接收到的文件位于Receiver.exe的同一目录下。