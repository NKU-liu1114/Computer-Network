# Computer-Network
南开大学计算机网络2023-2024FALL课程实验代码。
## 实验一
用 `Socket` 套接字实现了一个多人在线聊天程序，最终得分：97
## 实验二
网络抓包，用HTML写一个网页。在本地网络中用浏览器访问网页，用 `WireShark` 抓包捕获交互过程。这一块由于助教提问两个都没答上来（大概就是关于TCP三次握手四次挥手的），因此摆了。最终得分：90
## 实验3-1
用UDP实现可靠传输，支持差错检测，超时重传、累计确认等功能。代码中没有传输时间和吞吐率等指标，得分低了点。最终得分：94
## 实验3-2 
3-1基础上，加入滑动窗口机制，基本就是GBN那一套，除了序列号没使用循环的之外。最终得分：95
## 实验3-3
3-2基础上，引入了选择确认机制，即接收端对失序的数据包进行缓存。但按照助教的意思，这个实验需要给每个包都开定时器。（类似于SR选择重传协议）我没有实现这一点，分比较低，不建议借鉴这次实验的。最终得分：91



**借鉴请给star~**

