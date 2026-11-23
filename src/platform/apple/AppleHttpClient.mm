#import <Foundation/Foundation.h>

#include "HttpClient.hpp"

namespace blitzdj
{
    namespace
    {
        class AppleHttpClient final : public HttpClient
        {
        public:
            AppleHttpClient()
            {
                @autoreleasepool
                {
                    NSURLSessionConfiguration* configuration =
                        [NSURLSessionConfiguration ephemeralSessionConfiguration];

                    configuration.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
                    session_ = [NSURLSession sessionWithConfiguration:configuration];
                }
            }

            ~AppleHttpClient() override
            {
                [session_ finishTasksAndInvalidate];
            }

            HttpResponse send(const HttpRequest& request) override
            {
                HttpResponse response;

                @autoreleasepool
                {
                    NSString* urlString = [NSString stringWithUTF8String:request.url.c_str()];
                    NSURL* url = urlString ? [NSURL URLWithString:urlString] : nil;

                    if (!url)
                    {
                        response.error = "Invalid request URL.";
                        return response;
                    }

                    NSMutableURLRequest* urlRequest = [NSMutableURLRequest
                        requestWithURL:url
                        cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                        timeoutInterval:request.timeoutSeconds];

                    urlRequest.HTTPMethod = [NSString stringWithUTF8String:request.method.c_str()];

                    for (const auto& header : request.headers)
                    {
                        [urlRequest
                            setValue:[NSString stringWithUTF8String:header.second.c_str()]
                            forHTTPHeaderField:[NSString stringWithUTF8String:header.first.c_str()]];
                    }

                    const bool sendsBody = !request.body.empty() ||
                        request.method == "POST" || request.method == "PUT";

                    if (sendsBody)
                    {
                        urlRequest.HTTPBody = [NSData dataWithBytes:request.body.data() length:request.body.size()];
                    }

                    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

                    __block NSData* resultData = nil;
                    __block NSHTTPURLResponse* resultResponse = nil;
                    __block NSError* resultError = nil;

                    NSURLSessionDataTask* task = [session_
                        dataTaskWithRequest:urlRequest
                        completionHandler:^(NSData* data, NSURLResponse* urlResponse, NSError* error)
                        {
                            resultData = data;
                            resultError = error;

                            if ([urlResponse isKindOfClass:[NSHTTPURLResponse class]])
                            {
                                resultResponse = (NSHTTPURLResponse*)urlResponse;
                            }

                            dispatch_semaphore_signal(semaphore);
                        }];

                    [task resume];
                    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);

                    if (resultError)
                    {
                        const char* description = resultError.localizedDescription.UTF8String;
                        response.error = description ? description : "Request failed.";
                    }

                    if (resultResponse)
                    {
                        response.statusCode = static_cast<long>(resultResponse.statusCode);

                        NSString* retryAfter = [resultResponse valueForHTTPHeaderField:@"Retry-After"];

                        if (retryAfter)
                        {
                            response.retryAfterSeconds = retryAfter.intValue;
                        }
                    }

                    if (resultData.length > 0)
                    {
                        response.body.assign(
                            static_cast<const char*>(resultData.bytes),
                            resultData.length
                        );
                    }
                }

                return response;
            }

        private:
            NSURLSession* session_ = nil;
        };
    }

    std::unique_ptr<HttpClient> createDefaultHttpClient()
    {
        return std::make_unique<AppleHttpClient>();
    }
}
