#include <bits/stdc++.h>
#include "trpc/Client.h"

std::atomic<int> gTODO {};
std::atomic<std::chrono::system_clock::time_point> gStart {std::chrono::system_clock::now()};
std::atomic<std::chrono::system_clock::time_point> gEnd {std::chrono::system_clock::now()};

void clientRoutine(int sessions, int startNumber, int numbers) {
::signal(SIGPIPE, SIG_IGN);
    auto &env = co::open();
    int sum = startNumber;
    int count = 5e5;
    int done = 0;
    auto start = std::chrono::steady_clock::now();
    for(int i = 0; i < sessions; ++i) {
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
            for(size_t j = 0; j < numbers; ++j) {
                int old = sum;
                // add first, other clients won't send the same request
                sum++;
                auto resp = client.call<int>("add", old, 1);
                if(old + 1 != resp.value()) {
                    std::cerr << "failed at: " << i << ":" << j << std::endl;
                }
            }

            done++;
            if(done == sessions) {
                if(--gTODO == 0) {
                    gEnd = std::chrono::system_clock::now();
                    std::cout << "done, press any key" << std::endl;
                }
            }
        });
        co->resume();
    }
    
    co::loop();
}

int main(int argc, const char *argv[]) {
    int threads = 1;
    if(argc > 1) {
        threads = ::atoi(argv[1]);
    }
    gTODO = threads;

    // per thread
    int sessions = 1;
    if(argc > 2) {
        sessions = ::atoi(argv[2]);
    }

    // total
    int count = 1e5;
    if(argc > 3) {
        count = ::atoi(argv[3]);
    }
    gStart = std::chrono::system_clock::now();

    for(int i = 0; i < threads; ++i) {
        std::thread {clientRoutine, sessions, i * count, count / threads}.detach();
    }

    int c; std::cin>>c;

    using Milli = std::chrono::duration<double, std::milli>;
    auto perMsg = Milli{gEnd.load() - gStart.load()}.count() / count / threads;
    auto qps = 1000 / perMsg;
    std::cout << "per msg: " << perMsg << "ms" << std::endl;
    std::cout << "QPS: " << qps << std::endl;
    // 4750U
    // server: 10
    // client: 8 2 100000
    // per msg: 0.00633958ms
    // QPS: 157739
    return 0;
}