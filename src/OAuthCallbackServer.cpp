#include "OAuthCallbackServer.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace blitzdj
{
    namespace
    {
#ifdef _WIN32
        using NativeSocket = SOCKET;
        constexpr NativeSocket INVALID = INVALID_SOCKET;

        void closeSocket(NativeSocket socket)
        {
            closesocket(socket);
        }
#else
        using NativeSocket = int;
        constexpr NativeSocket INVALID = -1;

        void closeSocket(NativeSocket socket)
        {
            close(socket);
        }
#endif

        NativeSocket toNative(std::intptr_t value)
        {
            return static_cast<NativeSocket>(value);
        }

        struct ParsedUri
        {
            std::string host;
            std::string path;
            int port = 0;
            bool valid = false;
        };

        ParsedUri parse(const std::string& uri)
        {
            ParsedUri result;
            const std::string scheme = "http://";

            if (uri.rfind(scheme, 0) != 0)
            {
                return result;
            }

            const std::string rest = uri.substr(scheme.size());
            const auto slash = rest.find('/');
            const std::string authority = rest.substr(0, slash);

            result.path = slash == std::string::npos ? "/" : rest.substr(slash);

            const auto query = result.path.find_first_of("?#");

            if (query != std::string::npos)
            {
                result.path = result.path.substr(0, query);
            }

            const auto colon = authority.rfind(':');
            result.host = authority.substr(0, colon);

            if (colon == std::string::npos)
            {
                result.port = 80;
            }
            else
            {
                try
                {
                    result.port = std::stoi(authority.substr(colon + 1));
                }
                catch (...)
                {
                    return result;
                }
            }

            result.valid = (result.host == "127.0.0.1" || result.host == "localhost") &&
                result.port > 0 && result.port < 65536;

            return result;
        }

        void sendAll(NativeSocket socket, const std::string& data)
        {
            std::size_t sent = 0;

            while (sent < data.size())
            {
                const auto count = send(socket, data.data() + sent, static_cast<int>(data.size() - sent), 0);

                if (count <= 0)
                {
                    return;
                }

                sent += static_cast<std::size_t>(count);
            }
        }

        std::string page(const std::string& title, const std::string& message)
        {
            return "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
                   "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                   "<title>BlitzDJ</title><style>"
                   "body{margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;"
                   "background:#0e0f13;color:#f4f4f6;font-family:system-ui,sans-serif;text-align:center}"
                   "h1{color:#ffc21a;margin-bottom:8px}p{color:#9a9ca8}</style></head><body><div>"
                   "<h1>" + title + "</h1><p>" + message + "</p></div></body></html>";
        }

        void respond(NativeSocket socket, int status, const std::string& reason, const std::string& body)
        {
            sendAll(socket,
                "HTTP/1.1 " + std::to_string(status) + " " + reason + "\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Content-Length: " + std::to_string(body.size()) + "\r\n"
                "Cache-Control: no-store\r\n"
                "Connection: close\r\n\r\n" + body);
        }
    }

    OAuthCallbackServer::OAuthCallbackServer(const std::string& redirectUri)
    {
        const auto uri = parse(redirectUri);

        host_ = uri.host;
        path_ = uri.path;
        port_ = uri.port;
        valid_ = uri.valid;
    }

    OAuthCallbackServer::~OAuthCallbackServer()
    {
        stop();
    }

    bool OAuthCallbackServer::supports(const std::string& redirectUri)
    {
        return parse(redirectUri).valid;
    }

    bool OAuthCallbackServer::start()
    {
        if (isListening())
        {
            return true;
        }

        if (!valid_)
        {
            lastError_ = "Automatic login needs a http://127.0.0.1:<port> redirect URI.";
            return false;
        }

#ifdef _WIN32
        if (!networkReady_)
        {
            WSADATA data;

            if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
            {
                lastError_ = "Could not initialize Windows sockets.";
                return false;
            }

            networkReady_ = true;
        }
#else
        networkReady_ = true;
#endif

        NativeSocket listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (listener == INVALID)
        {
            lastError_ = "Could not create the login callback socket.";
            return false;
        }

#ifndef _WIN32
        int reuse = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(static_cast<std::uint16_t>(port_));
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
            listen(listener, 4) != 0)
        {
#ifdef _WIN32
            const int code = WSAGetLastError();
            const bool reserved = code == WSAEACCES;
#else
            const int code = errno;
            const bool reserved = code == EACCES;
#endif
            closeSocket(listener);
            lastError_ = reserved
                ? "Port " + std::to_string(port_) + " is reserved by the system."
                : "Port " + std::to_string(port_) + " is already in use by another program.";
            return false;
        }

        socket_ = static_cast<std::intptr_t>(listener);
        lastError_.clear();
        return true;
    }

    void OAuthCallbackServer::stop()
    {
        if (isListening())
        {
            closeSocket(toNative(socket_));
            socket_ = -1;
        }

        if (networkReady_)
        {
#ifdef _WIN32
            WSACleanup();
#endif
            networkReady_ = false;
        }
    }

    bool OAuthCallbackServer::isListening() const
    {
        return socket_ != -1;
    }

    std::optional<std::string> OAuthCallbackServer::waitForRedirect(
        std::chrono::seconds timeout,
        const std::atomic<bool>* cancel
    )
    {
        if (!isListening() && !start())
        {
            return std::nullopt;
        }

        const auto deadline = std::chrono::steady_clock::now() + timeout;
        const NativeSocket listener = toNative(socket_);

        while (std::chrono::steady_clock::now() < deadline)
        {
            if (cancel && cancel->load())
            {
                lastError_ = "Login was cancelled.";
                return std::nullopt;
            }

            fd_set readable;
            FD_ZERO(&readable);
            FD_SET(listener, &readable);

            timeval wait{};
            wait.tv_sec = 0;
            wait.tv_usec = 250000;

            const int ready = select(static_cast<int>(listener) + 1, &readable, nullptr, nullptr, &wait);

            if (ready <= 0)
            {
                continue;
            }

            const NativeSocket client = accept(listener, nullptr, nullptr);

            if (client == INVALID)
            {
                continue;
            }

            auto redirect = handleClient(static_cast<std::intptr_t>(client));
            closeSocket(client);

            if (redirect)
            {
                return redirect;
            }
        }

        lastError_ = "Timed out waiting for the Spotify login.";
        return std::nullopt;
    }

    std::optional<std::string> OAuthCallbackServer::handleClient(std::intptr_t clientHandle)
    {
        const NativeSocket client = toNative(clientHandle);

#ifdef _WIN32
        DWORD receiveTimeout = 5000;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&receiveTimeout), sizeof(receiveTimeout));
#else
        timeval receiveTimeout{};
        receiveTimeout.tv_sec = 5;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &receiveTimeout, sizeof(receiveTimeout));
#endif

        std::string request;
        char buffer[2048];

        while (request.find("\r\n\r\n") == std::string::npos && request.size() < 16384)
        {
            const auto count = recv(client, buffer, sizeof(buffer), 0);

            if (count <= 0)
            {
                break;
            }

            request.append(buffer, static_cast<std::size_t>(count));
        }

        const auto lineEnd = request.find("\r\n");
        const std::string line = request.substr(0, lineEnd);

        const auto firstSpace = line.find(' ');
        const auto secondSpace = line.find(' ', firstSpace + 1);

        if (firstSpace == std::string::npos || secondSpace == std::string::npos ||
            line.substr(0, firstSpace) != "GET")
        {
            respond(client, 400, "Bad Request", page("BlitzDJ", "Invalid request."));
            return std::nullopt;
        }

        const std::string target = line.substr(firstSpace + 1, secondSpace - firstSpace - 1);
        const std::string targetPath = target.substr(0, target.find('?'));

        if (targetPath != path_)
        {
            respond(client, 404, "Not Found", page("BlitzDJ", "Not found."));
            return std::nullopt;
        }

        const bool denied = target.find("error=") != std::string::npos;

        if (denied)
        {
            respond(client, 200, "OK", page("Login cancelled", "You can close this tab and return to BlitzDJ."));
        }
        else
        {
            respond(client, 200, "OK", page("Spotify authorized", "You can close this tab and return to BlitzDJ."));
        }

        return "http://" + host_ + ":" + std::to_string(port_) + target;
    }

    const std::string& OAuthCallbackServer::getLastError() const
    {
        return lastError_;
    }
}
