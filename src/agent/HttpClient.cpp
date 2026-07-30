#include "agent/HttpClient.h"

#include "agent/StringUtil.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include <format>
#include <string>
#include <string_view>

namespace meshgate_agent {
namespace {

std::string jsonEscape(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (char const ch : value) {
        if (ch == '"' || ch == '\\') out.push_back('\\');
        out.push_back(ch);
    }
    return out;
}

class WsaSession {
public:
    WsaSession() : mOk(WSAStartup(MAKEWORD(2, 2), &mData) == 0) {}
    ~WsaSession() {
        if (mOk) WSACleanup();
    }

    [[nodiscard]] bool ok() const { return mOk; }

private:
    WSADATA mData{};
    bool    mOk;
};

class SocketHandle {
public:
    explicit SocketHandle(SOCKET socket) : mSocket(socket) {}
    ~SocketHandle() {
        if (mSocket != INVALID_SOCKET) ::closesocket(mSocket);
    }

    SocketHandle(SocketHandle const&)            = delete;
    SocketHandle& operator=(SocketHandle const&) = delete;

    [[nodiscard]] SOCKET get() const { return mSocket; }

private:
    SOCKET mSocket;
};

} // namespace

bool postHandoff(
    AgentConfig const& config,
    std::string_view   xuid,
    std::string_view   playerName,
    std::string_view   target,
    std::string&       response
) {
    WsaSession const wsa;
    if (!wsa.ok()) {
        response = "WSAStartup failed";
        return false;
    }

    SocketHandle socket{::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
    if (socket.get() == INVALID_SOCKET) {
        response = "could not create socket";
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port   = htons(config.apiPort);

    auto const host = config.apiHost == "localhost" ? std::string{"127.0.0.1"} : config.apiHost;
    if (InetPtonA(AF_INET, host.c_str(), &address.sin_addr) != 1) {
        response = "apiHost must be an IPv4 literal or localhost";
        return false;
    }

    if (::connect(socket.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
        response = "could not connect to MeshGate control API";
        return false;
    }

    auto const body = std::format(
        R"({{"xuid":"{}","playerName":"{}","targetServer":"{}"}})",
        jsonEscape(xuid),
        jsonEscape(playerName),
        jsonEscape(target)
    );
    auto const request = std::format(
        "POST /handoff HTTP/1.1\r\n"
        "Host: {}:{}\r\n"
        "Authorization: Bearer {}\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: {}\r\n"
        "Connection: close\r\n\r\n{}",
        config.apiHost,
        config.apiPort,
        config.bearerToken,
        body.size(),
        body
    );

    int const sent = ::send(socket.get(), request.data(), static_cast<int>(request.size()), 0);
    if (sent <= 0) {
        response = "could not send handoff request";
        return false;
    }

    std::string raw;
    raw.resize(8192);

    int const received = ::recv(socket.get(), raw.data(), static_cast<int>(raw.size()), 0);
    if (received <= 0) {
        response = "MeshGate closed the control connection without a response";
        return false;
    }

    raw.resize(static_cast<std::size_t>(received));
    response = raw;

    return raw.starts_with("HTTP/1.1 200") || raw.starts_with("HTTP/1.0 200");
}

} // namespace meshgate_agent
