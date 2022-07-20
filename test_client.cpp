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
        auto start = std::chrono::steady_clock::now();
        auto client = std::move(pClient.value());
        int sum = 0;
        constexpr size_t count = 1e5;
        for(size_t i = 0; i < count; ++i) {
            auto resp = client.call<int>("add", sum, 1);
            int old = sum;
            if(old == (sum = resp.value_or(sum))) {
                std::cerr << "failed at: " << i << std::endl;
            }
        }
        auto end = std::chrono::steady_clock::now();
        std::cout << "sum: " << sum << std::endl;
        using Milli = std::chrono::duration<double, std::milli>;
        std::cout << "per msg: " << Milli{end - start}.count() / count << "ms" << std::endl;
        // output:
        // connected
        // sum: 100000
        // per msg: 0.341057ms
    });
    co->resume();
    co::loop();
    return 0;
}