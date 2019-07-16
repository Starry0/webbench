# webbench

Webbench是一个在Linux下使用的网站压力测试工具。使用fork()模拟多个客户端访问


##命令行选项：




| 短参        | 长参数           | 作用   |
| ------------- |:-------------:| -----:|
|-f     |--force                |不需要等待服务器响应               | 
|-r     |--reload               |发送重新加载请求                   |
|-t     |--time <sec>           |运行多长时间，单位：秒"            |
|-p     |--proxy <server:port>  |使用代理服务器访问          |
|-c     |--clients <n>          |创建的客户端数量            |
|-9     |--http09               |使用HTTP/0.9协议            |
|-1     |--http10               |使用HTTP/1.0协议           |
|-2     |--http11               |使用HTTP/1.1协议           |
|       |--get                  |使用GET请求方法            |
|       |--head                 |使用HEAD请求方法            |
|       |--options              |使用OPTIONS请求方法            |
|       |--trace                |使用TRACE请求方法|
|-?/-h  |--help                 |打印帮助文档            |
|-V     |--version              |显示版本号            |