#pragma once
#include <netinet/in.h>
#include <cstddef>
#include "vsjson.hpp"
#include "protocol.h"
namespace trpc {
namespace detail {

class Codec {
public:
    using Header = uint32_t;
    constexpr static Header INVALID_HEADER = -1;

    std::tuple<bool, Header> contentLength(const char *buf, size_t N) const {
        if(N < sizeof(uint32_t)) return {false, INVALID_HEADER};
        Header beLength = *(Header*)(buf);
        Header length = ::ntohl(beLength);
        return {true, length};
    }

    bool verify(const char *buf, size_t N) const {
        auto [succ, length] = contentLength(buf, N);
        return succ && N >= sizeof(uint32_t) + length;
    }

    vsjson::Json decode(const char *buf, size_t N) const {
        return vsjson::parse(buf + sizeof(uint32_t));
    }

    void reportError(vsjson::Json &response, const protocol::Exception &e) {
        if(response.contains(protocol::Field::result)) {
            auto &obj = response.as<vsjson::ObjectImpl>();
            obj.erase(protocol::Field::result);
        }
        response[protocol::Field::error] = {
            {protocol::Field::code, e.code()}
        };
        if(!e.message().empty()) {
            auto &err = response[protocol::Field::error];
            err[protocol::Field::message] = e.message();
        }
    }

    std::tuple<std::string, Header, Header> dump(vsjson::Json &response) {
        std::string dump = response.dump();
        Header responseLength = dump.length();
        Header responseLengthBeLength = ::htonl(responseLength);
        return {std::move(dump), responseLength, responseLengthBeLength};
    }
};

} // detail
} // trpc
