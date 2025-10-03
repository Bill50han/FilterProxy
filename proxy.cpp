#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <vector>
#include <iostream>

#define BUFFER_SIZE 4096

// 处理 server -> client 数据
void handle_server_to_client(int client_fd, int server_fd, const std::string &keyword, int option) {
    char buffer[BUFFER_SIZE];
    ssize_t n;
    while ((n = read(server_fd, buffer, BUFFER_SIZE)) > 0) {
        std::string data(buffer, n);

        // 处理 keyword
        auto pos = data.find(keyword);
        while (pos != std::string::npos) {
            if (option == 0) {
                // RST: 直接关闭客户端
                close(client_fd);
                close(server_fd);
                return;
            } else if (option == 1) {
                // 替换 keyword + 后 20 个字符
                size_t len = std::min<size_t>(20 + keyword.size(), data.size() - pos);
                data.replace(pos, len, len, '*');
            }
            pos = data.find(keyword);
        }

        // 发给客户端
        ssize_t sent = 0;
        while (sent < (ssize_t)data.size()) {
            ssize_t s = write(client_fd, data.data() + sent, data.size() - sent);
            if (s <= 0) break;
            sent += s;
        }
    }
    close(client_fd);
    close(server_fd);
}

void handle_client_to_server(int client_fd, int server_fd) {
    char buf[BUFFER_SIZE];
    ssize_t n;
    while ((n = read(client_fd, buf, BUFFER_SIZE)) > 0) {
        std::string data(buf, n);
        // 找到请求头 Accept-Encoding
        std::string target = "Accept-Encoding:";
        size_t pos = data.find(target);
        if (pos != std::string::npos) {
            size_t end = data.find("\r\n", pos);
            if (end != std::string::npos) {
                // 改为 identity
                std::string new_header = "Accept-Encoding: identity";
                data.replace(pos, end - pos, new_header);
            }
        }
        ssize_t sent = 0;
        while (sent < (ssize_t)data.size()) {
            ssize_t s = write(server_fd, data.data() + sent, data.size() - sent);
            if (s <= 0) break;
            sent += s;
        }
    }
    shutdown(server_fd, SHUT_WR);
}

// 每个客户端线程
void handle_client(int client_fd, const sockaddr_in &server_addr, const std::string &keyword, int option) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return;

    if (connect(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(client_fd);
        close(server_fd);
        return;
    }

    // 这里客户端->服务端不做处理，直接转发
    //std::thread t1([client_fd, server_fd]() {
    //    char buffer[BUFFER_SIZE];
    //    ssize_t n;
    //    while ((n = read(client_fd, buffer, BUFFER_SIZE)) > 0) {
    //        ssize_t sent = 0;
    //        while (sent < n) {
    //            ssize_t s = write(server_fd, buffer + sent, n - sent);
    //            if (s <= 0) break;
    //            sent += s;
    //        }
    //    }
    //    shutdown(server_fd, SHUT_WR); // 发送完毕
    //});

    //去掉gzip
    std::thread t1(handle_client_to_server, client_fd, server_fd);

    // server->client，处理 keyword
    handle_server_to_client(client_fd, server_fd, keyword, option);

    t1.join();
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " [source port] [dest port] [KeyWord] [option]\n";
        return 1;
    }

    int src_port = atoi(argv[1]);
    int dst_port = atoi(argv[2]);
    std::string keyword = argv[3];
    int option = atoi(argv[4]);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) return 1;

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(src_port);

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(listen_fd, (sockaddr*)&addr, sizeof(addr)) < 0) return 1;
    if (listen(listen_fd, 128) < 0) return 1;

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // 或原始服务 IP
    server_addr.sin_port = htons(dst_port);

    std::vector<std::thread> threads;

    while (true) {
        sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (sockaddr*)&client_addr, &len);
        if (client_fd >= 0) {
            threads.emplace_back(handle_client, client_fd, server_addr, keyword, option);
            // 清理已完成线程
            for (auto it = threads.begin(); it != threads.end();) {
                if (it->joinable() && it->get_id() != std::this_thread::get_id()) {
                    it->join();
                    it = threads.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    close(listen_fd);
    return 0;
}
