`Usage: ./proxy [source_port] [dest_port] [KeyWord] [option]`

程序会监听 0.0.0.0:source_port ，将请求转发到 127.0.0.1:dest_port，并阻止使用 gzip 压缩，且在 server -> client 的内容中检测到 KeyWord 时，    
当 option == 0  
- 关闭连接，

或当 option == 1  
- 将 KeyWord 及其后20个字符替换成 `*`

支持 HTTP/1.*，不支持 HTTP/3.0，HTTP/2.0 未测试。

---

编译：
```sh
g++ -std=c++17 -O2 -Wall -Wextra proxy.cpp -o proxy
```
