#include <bits/stdc++.h>
#include "trpc/Server.h"

std::string append(std::string a, std::string b) {
    return a+b;
}

void serverRoutine() {
    ::signal(SIGPIPE, SIG_IGN);
    auto pServer = trpc::Server::make({"127.0.0.1", 2333});
    std::cout << bool(pServer) << std::endl;
    if(!pServer) {
        std::cerr << "cannot create server." << std::endl;
    }
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

int main(int argc, const char *argv[]) {
    int threads = 1;
    if(argc > 1) {
        threads = ::atoi(argv[1]);
    }
    for(int i = 1; i < threads; ++i) {
        std::thread {serverRoutine}.detach();
    }
    serverRoutine();
}

// R7 4750U
// server: 16 threads

////////////////////////////////

// start test: {threads: 8, per thread sessions: 1, total sessions: 8, count: 65536}
// per msg: 0.0487008ms
// QPS: 20533.6

// start test: {threads: 8, per thread sessions: 2, total sessions: 16, count: 65536}
// per msg: 0.0311257ms
// QPS: 32127.8

// start test: {threads: 8, per thread sessions: 16, total sessions: 128, count: 65536}
// per msg: 0.0135604ms
// QPS: 73743.9

// start test: {threads: 8, per thread sessions: 32, total sessions: 256, count: 65536}
// per msg: 0.00995204ms
// QPS: 100482

// start test: {threads: 8, per thread sessions: 64, total sessions: 512, count: 65536}
// per msg: 0.0112954ms
// QPS: 88531.9

// start test: {threads: 8, per thread sessions: 128, total sessions: 1024, count: 65536}
// per msg: 0.0128362ms
// QPS: 77904.5

// start test: {threads: 8, per thread sessions: 256, total sessions: 2048, count: 65536}
// per msg: 0.0154712ms
// QPS: 64636.4

////////////////////////////////

// start test: {threads: 16, per thread sessions: 1, total sessions: 16, count: 65536}
// per msg: 0.0297828ms
// QPS: 33576.4

// start test: {threads: 16, per thread sessions: 2, total sessions: 32, count: 65536}
// per msg: 0.0218523ms
// QPS: 45761.7

// start test: {threads: 16, per thread sessions: 4, total sessions: 64, count: 65536}
// per msg: 0.0142998ms
// QPS: 69931.1

// start test: {threads: 16, per thread sessions: 8, total sessions: 128, count: 65536}
// per msg: 0.00985372ms
// QPS: 101485

// start test: {threads: 16, per thread sessions: 16, total sessions: 256, count: 65536}
// per msg: 0.00956105ms
// QPS: 104591

// start test: {threads: 16, per thread sessions: 32, total sessions: 512, count: 65536}
// per msg: 0.0102948ms
// QPS: 97136

// start test: {threads: 16, per thread sessions: 64, total sessions: 1024, count: 65536}
// per msg: 0.0145101ms
// QPS: 68917.4

// start test: {threads: 16, per thread sessions: 128, total sessions: 2048, count: 65536}
// per msg: 0.0175922ms
// QPS: 56843.5

////////////////////////////////

// start test: {threads: 32, per thread sessions: 1, total sessions: 32, count: 65536}
// per msg: 0.0206534ms
// QPS: 48418.3

// start test: {threads: 32, per thread sessions: 2, total sessions: 64, count: 65536}
// per msg: 0.0171483ms
// QPS: 58314.7

// start test: {threads: 32, per thread sessions: 4, total sessions: 128, count: 65536}
// per msg: 0.0102365ms
// QPS: 97689.8

// start test: {threads: 32, per thread sessions: 8, total sessions: 256, count: 65536}
// per msg: 0.0112331ms
// QPS: 89022.5

// start test: {threads: 32, per thread sessions: 16, total sessions: 512, count: 65536}
// per msg: 0.0100133ms
// QPS: 99867.3

// start test: {threads: 32, per thread sessions: 32, total sessions: 1024, count: 65536}
// per msg: 0.0120643ms
// QPS: 82889

// start test: {threads: 32, per thread sessions: 64, total sessions: 2048, count: 65536}
// per msg: 0.0173144ms
// QPS: 57755.4
