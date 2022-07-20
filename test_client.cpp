#include <bits/stdc++.h>
#include "trpc/Client.h"

int main() {
    ::signal(SIGPIPE, SIG_IGN);
    auto &env = co::open();
    auto co = env.createCoroutine([&] {
        trpc::Endpoint peer {"127.0.0.1", 2333};
        auto pClient = trpc::Client::make(peer);
        if(!pClient) {
            std::cerr << "failed, abort" << std::endl;
            return;
        } else {
            std::cout << "connected" << std::endl;
        }
        auto client = std::move(pClient.value());
        auto pResult = client.call<std::string>("append", "jojo", "dio");
        if(pResult) {
            std::cout << pResult.value() << std::endl;
        } else {
            std::cerr << "failed" << std::endl;
            std::cerr << "reason:" << strerror(client.error()) << std::endl;
        }
        auto inval = client.call<int>("add", 1, 2, 3);
        if(!inval) {
            std::cerr << "inval failed" << std::endl;
        } else {
            std::cout << inval.value() << std::endl;
        }
        auto pResult2 = client.call<int>("add", 1, 2);
        std::cout << pResult2.value_or(123) << std::endl;
    });
    co->resume();
    co::loop();
    return 0;
}