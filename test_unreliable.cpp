#include <algorithm>
#include <chrono>
#include <random>
#include <memory>
#include "trpc/Server.h"
#include "trpc/Client.h"

using namespace std::chrono;
using namespace std::chrono_literals;

struct Mock {
    milliseconds rttMin;
    milliseconds rttMax; // >= rttMin
    size_t lostRate; // [0, 100]
    size_t crashRate; // [0, 100], I will kill my self!
    size_t packetLimit; // [0, packetLimit) bytes
};

trpc::Endpoint local {"127.0.0.1", 2334};

int main(int argc, const char *argv[]) {
    ::signal(SIGPIPE, SIG_IGN);
    auto &env = co::open();
    using Milli = duration<double, std::milli>;


    /// configurations


    // server virtual network
    constexpr Mock mock {
        .rttMin = 100ms,
        .rttMax = 300ms,
        .lostRate = 40,
        .crashRate = 5,
        // TODO
        .packetLimit = std::numeric_limits<size_t>::max()
    };

    // client config
    constexpr int sessions = 100;
    constexpr int numbers = sessions * 2;
    constexpr auto timeout = 150ms;

    std::random_device randomDevice;
    std::mt19937 randomEngine(randomDevice());
    std::uniform_int_distribution<> distRTT(mock.rttMin.count(), mock.rttMax.count());
    std::uniform_int_distribution<> distLost(0, 100);
    auto randRTT = [&] { return distRTT(randomEngine); };
    auto randLost = [&] { return distLost(randomEngine); };
    auto randCrash = randLost;


    /// create server


    auto pServer = trpc::Server::make(local);
    if(!pServer) {
        std::cerr << "cannot create server." << std::endl;
        return 1;
    }
    auto server = std::move(pServer.value());


    /// bind service


    server.bind("add", [](int a, int b) { return a + b; });


    /// virtual network


    server.onRequest([&](auto &&) {
        if(randCrash() < mock.crashRate) {
            server.close();
            return false;
        }
        if(milliseconds delay {randRTT()}; delay != 0ms) {
            co::poll(nullptr, 0, delay.count());
        }
        bool lost = randLost() < mock.lostRate;
        return !lost;
    });


    /// start server routine


    env.createCoroutine([&server] {
        server.start();
    })->resume();


    /// client routine


    constexpr int sNumbers = numbers / sessions;

    int sum = 0;
    int done = 0;
    int fail = 0;
    int succ = 0;
    auto start = steady_clock::now();

    bool firstTest = true;
    Milli minLatency;
    Milli maxLatency;

    auto report = [&] {
        auto end = steady_clock::now();
        auto elapsed = Milli{end - start}.count();
        auto amortized = elapsed / numbers;
        std::cout << "======================" << std::endl;
        std::cout << "elapsed: " << elapsed << "ms" << std::endl;
        std::cout << "amortized: " << amortized << "ms" << std::endl;
        std::cout << "min: " << minLatency.count() << "ms" << std::endl;
        std::cout << "max: " << maxLatency.count() << "ms" << std::endl;
        // ms -> s
        std::cout << "QPS: " << 1000.0 / amortized << std::endl;
        std::cout << "succ: " << succ << std::endl;
        std::cout << "fail: " << fail << std::endl;
    };


    for(int i = 0; i < sessions; ++i) {
        auto co = env.createCoroutine([&, i] {
            auto pClient = trpc::Client::make(local);
            if(!pClient) {
                std::cerr << "client failed, abort this session: " << i << std::endl;
                return;
            }
            auto client = std::move(pClient.value());
            client.setTimeout(timeout);
            for(size_t j = 0; j < sNumbers; ++j) {
                int old = sum++;


                /// call


                auto respStart = steady_clock::now();
                auto resp = client.call<int>("add", old, 1);
                auto respEnd = steady_clock::now();

                if(!resp || old + 1 != resp.value()) {
                    std::cerr << "failed at: " << i << ":" << j << std::endl;
                    fail++;
                } else {
                    succ++;
                }

                if(firstTest) {
                    firstTest = false;
                    minLatency = maxLatency = Milli{respEnd - respStart};
                // closed client or inconsistent connection will return `call()` quickly
                } else if(Milli latency {respEnd - respStart}; latency > 0.1ms){
                    minLatency = std::min(minLatency, latency);
                    maxLatency = std::max(maxLatency, latency);
                }
            }

            if(++done == sessions) {
                report();
            }
        });
        co->resume();
    }


    co::loop();
}