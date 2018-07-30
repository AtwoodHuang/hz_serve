高性能web服务器，大概3000多行。支持GET， POST方法。支持部分首部字段，支持PHP。用了EPOLL的ET模式，内存池，线程池，定时器，非阻塞io，
用最小堆实现定时器。之前没有加PHP解析部分时用webbench做压力测试，10000并发链接访问30秒毫无问题，现在更换电脑且加入PHP解析部分后性能下降，原因还需一段
事件找出。注意在配置php——fpm时把用户和用户组设为你自己的用户。下面是访问静态和动态页面截图
![image](https://github.com/AtwoodHuang/hz_serve/blob/master/html/2018-07-30%2020-27-09%E5%B1%8F%E5%B9%95%E6%88%AA%E5%9B%BE.png)
![image](https://github.com/AtwoodHuang/hz_serve/blob/master/html/2018-07-30%2020-27-38%E5%B1%8F%E5%B9%95%E6%88%AA%E5%9B%BE.png)
![image](https://github.com/AtwoodHuang/hz_serve/blob/master/html/2018-07-30%2021-19-08%E5%B1%8F%E5%B9%95%E6%88%AA%E5%9B%BE.png)
