#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace blitzdj
{
    struct HttpRequest
    {
        std::string method = "GET";
        std::string url;
        std::vector<std::pair<std::string, std::string>> headers;
        std::string body;
        int timeoutSeconds = 30;
    };

    struct HttpResponse
    {
        long statusCode = 0;
        std::string body;
        std::string error;
        int retryAfterSeconds = 0;
    };

    class HttpClient
    {
    public:
        virtual ~HttpClient() = default;

        virtual HttpResponse send(const HttpRequest& request) = 0;
    };

    std::unique_ptr<HttpClient> createDefaultHttpClient();
}
