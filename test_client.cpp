#include <bits/stdc++.h>
#include "trpc/Client.h"

using namespace std::chrono;

std::atomic<int> gTODO {};
std::atomic<system_clock::time_point> gStart {system_clock::now()};
std::atomic<system_clock::time_point> gEnd {system_clock::now()};

void clientRoutine(int sessions, int startNumber, int numbers) {
    ::signal(SIGPIPE, SIG_IGN);
    auto &env = co::open();
    int sum = startNumber;
    int done = 0;
    int sNumbers = numbers / sessions;
    for(int i = 0; i < sessions; ++i) {
        auto co = env.createCoroutine([&] {
            trpc::Endpoint peer {"127.0.0.1", 2333};
            auto pClient = trpc::Client::make(peer);
            if(!pClient) {
                std::cerr << "failed, abort" << std::endl;
                return;
            }
            auto client = std::move(pClient.value());
            for(size_t j = 0; j < sNumbers; ++j) {
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
                    gEnd = system_clock::now();
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
    int count = 1 << 16;
    if(argc > 3) {
        count = ::atoi(argv[3]);
    }

    std::cout << "start test: " << '{'
        << "threads: " << threads << ", "
        << "per thread sessions: " << sessions << ", "
        << "total sessions: " << threads * sessions << ", "
        << "count: " << count << '}' << std::endl;

    gStart = system_clock::now();

    for(int i = 0; i < threads; ++i) {
        std::thread {clientRoutine, sessions, i * count, count / threads}.detach();
    }

    // hang
    int c; std::cin>>c;

    using Milli = duration<double, std::milli>;
    auto perMsg = Milli{gEnd.load() - gStart.load()}.count() / count;
    auto qps = 1000 / perMsg;
    std::cout << "per msg: " << perMsg << "ms" << std::endl;
    std::cout << "QPS: " << qps << std::endl;

    return 0;
}