#include "HttpClient.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <mutex>

namespace blitzdj
{
    namespace
    {
        std::once_flag curlInitFlag;

        std::size_t writeBody(char* data, std::size_t size, std::size_t count, void* user)
        {
            static_cast<std::string*>(user)->append(data, size * count);
            return size * count;
        }

        std::size_t readHeader(char* data, std::size_t size, std::size_t count, void* user)
        {
            const std::size_t length = size * count;
            const std::string line(data, length);
            const auto colon = line.find(':');

            if (colon != std::string::npos)
            {
                std::string name = line.substr(0, colon);

                std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c)
                {
                    return static_cast<char>(std::tolower(c));
                });

                if (name == "retry-after")
                {
                    try
                    {
                        *static_cast<int*>(user) = std::stoi(line.substr(colon + 1));
                    }
                    catch (...)
                    {
                    }
                }
            }

            return length;
        }

        class CurlHttpClient final : public HttpClient
        {
        public:
            CurlHttpClient()
            {
                std::call_once(curlInitFlag, []
                {
                    curl_global_init(CURL_GLOBAL_DEFAULT);
                });

                handle_ = curl_easy_init();
            }

            ~CurlHttpClient() override
            {
                if (handle_)
                {
                    curl_easy_cleanup(handle_);
                }
            }

            CurlHttpClient(const CurlHttpClient&) = delete;
            CurlHttpClient& operator=(const CurlHttpClient&) = delete;

            HttpResponse send(const HttpRequest& request) override
            {
                HttpResponse response;

                if (!handle_)
                {
                    response.error = "Could not create HTTP client.";
                    return response;
                }

                curl_easy_reset(handle_);

                curl_slist* headers = nullptr;

                for (const auto& [name, value] : request.headers)
                {
                    headers = curl_slist_append(headers, (name + ": " + value).c_str());
                }

                const bool sendsBody = !request.body.empty() ||
                    request.method == "POST" || request.method == "PUT";

                if (sendsBody)
                {
                    curl_easy_setopt(handle_, CURLOPT_POSTFIELDS, request.body.c_str());
                    curl_easy_setopt(handle_, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
                }

                curl_easy_setopt(handle_, CURLOPT_URL, request.url.c_str());
                curl_easy_setopt(handle_, CURLOPT_CUSTOMREQUEST, request.method.c_str());
                curl_easy_setopt(handle_, CURLOPT_HTTPHEADER, headers);
                curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, writeBody);
                curl_easy_setopt(handle_, CURLOPT_WRITEDATA, &response.body);
                curl_easy_setopt(handle_, CURLOPT_HEADERFUNCTION, readHeader);
                curl_easy_setopt(handle_, CURLOPT_HEADERDATA, &response.retryAfterSeconds);
                curl_easy_setopt(handle_, CURLOPT_TIMEOUT, static_cast<long>(request.timeoutSeconds));
                curl_easy_setopt(handle_, CURLOPT_CONNECTTIMEOUT, 10L);
                curl_easy_setopt(handle_, CURLOPT_FOLLOWLOCATION, 0L);
                curl_easy_setopt(handle_, CURLOPT_NOSIGNAL, 1L);
                curl_easy_setopt(handle_, CURLOPT_ACCEPT_ENCODING, "");

                const CURLcode code = curl_easy_perform(handle_);

                curl_easy_getinfo(handle_, CURLINFO_RESPONSE_CODE, &response.statusCode);
                curl_slist_free_all(headers);

                if (code != CURLE_OK)
                {
                    response.error = curl_easy_strerror(code);
                }

                return response;
            }

        private:
            CURL* handle_ = nullptr;
        };
    }

    std::unique_ptr<HttpClient> createDefaultHttpClient()
    {
        return std::make_unique<CurlHttpClient>();
    }
}
