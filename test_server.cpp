#include <bits/stdc++.h>
#include "trpc/Server.h"

std::string append(std::string a, std::string b) {
    return a+b;
}

int main() {
    ::signal(SIGPIPE, SIG_IGN);
    auto pServer = trpc::Server::make({"127.0.0.1", 2333});
    std::cout << bool(pServer) << std::endl;
    if(!pServer) return 1;
    auto server = std::move(pServer.value());
    using namespace std::chrono_literals;

    server.bind("add", [](int a, int b) { return a + b; });
    server.bind("append", append);

    auto &env = co::open();
    auto listener = env.createCoroutine([&server] {
        server.start();
    });
    listener->resume();
    co::loop();
}