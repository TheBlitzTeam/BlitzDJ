#include "HttpClient.hpp"
#include "AndroidJni.hpp"

#include <atomic>
#include <string>

namespace blitzdj
{
    namespace jni
    {
        namespace
        {
            std::atomic<JavaVM*> globalVM{nullptr};
        }

        JavaVM* javaVM()
        {
            return globalVM.load();
        }

        void setJavaVM(JavaVM* vm)
        {
            globalVM.store(vm);
        }
    }

    namespace
    {
        class EnvScope
        {
        public:
            EnvScope()
            {
                JavaVM* vm = jni::javaVM();

                if (!vm)
                {
                    return;
                }

                const jint status = vm->GetEnv(reinterpret_cast<void**>(&env_), JNI_VERSION_1_6);

                if (status == JNI_EDETACHED)
                {
                    if (vm->AttachCurrentThread(&env_, nullptr) == JNI_OK)
                    {
                        attached_ = true;
                    }
                    else
                    {
                        env_ = nullptr;
                    }
                }
                else if (status != JNI_OK)
                {
                    env_ = nullptr;
                }
            }

            ~EnvScope()
            {
                if (attached_)
                {
                    jni::javaVM()->DetachCurrentThread();
                }
            }

            EnvScope(const EnvScope&) = delete;
            EnvScope& operator=(const EnvScope&) = delete;

            JNIEnv* get() const
            {
                return env_;
            }

        private:
            JNIEnv* env_ = nullptr;
            bool attached_ = false;
        };

        std::string describeException(JNIEnv* env)
        {
            jthrowable exception = env->ExceptionOccurred();
            env->ExceptionClear();

            if (!exception)
            {
                return "Unknown Java exception.";
            }

            jclass throwableClass = env->FindClass("java/lang/Throwable");
            jmethodID toString = env->GetMethodID(throwableClass, "toString", "()Ljava/lang/String;");
            auto text = static_cast<jstring>(env->CallObjectMethod(exception, toString));

            if (env->ExceptionCheck() || !text)
            {
                env->ExceptionClear();
                return "Unknown Java exception.";
            }

            const char* chars = env->GetStringUTFChars(text, nullptr);
            std::string message = chars ? chars : "Unknown Java exception.";

            if (chars)
            {
                env->ReleaseStringUTFChars(text, chars);
            }

            return message;
        }

        std::string readStream(JNIEnv* env, jobject stream)
        {
            std::string output;

            if (!stream)
            {
                return output;
            }

            jclass streamClass = env->FindClass("java/io/InputStream");
            jmethodID read = env->GetMethodID(streamClass, "read", "([B)I");
            jmethodID close = env->GetMethodID(streamClass, "close", "()V");

            jbyteArray buffer = env->NewByteArray(8192);

            while (true)
            {
                const jint count = env->CallIntMethod(stream, read, buffer);

                if (env->ExceptionCheck())
                {
                    env->ExceptionClear();
                    break;
                }

                if (count <= 0)
                {
                    break;
                }

                const std::size_t start = output.size();
                output.resize(start + static_cast<std::size_t>(count));
                env->GetByteArrayRegion(buffer, 0, count, reinterpret_cast<jbyte*>(&output[start]));
            }

            env->CallVoidMethod(stream, close);

            if (env->ExceptionCheck())
            {
                env->ExceptionClear();
            }

            return output;
        }

        class AndroidHttpClient final : public HttpClient
        {
        public:
            HttpResponse send(const HttpRequest& request) override
            {
                HttpResponse response;
                EnvScope scope;
                JNIEnv* env = scope.get();

                if (!env)
                {
                    response.error = "Java VM is unavailable.";
                    return response;
                }

                if (env->PushLocalFrame(64) != JNI_OK)
                {
                    response.error = "Could not allocate JNI frame.";
                    return response;
                }

                perform(env, request, response);

                if (env->ExceptionCheck())
                {
                    response.error = describeException(env);
                }

                env->PopLocalFrame(nullptr);
                return response;
            }

        private:
            static bool failed(JNIEnv* env, HttpResponse& response)
            {
                if (!env->ExceptionCheck())
                {
                    return false;
                }

                response.error = describeException(env);
                response.statusCode = 0;
                return true;
            }

            static void perform(JNIEnv* env, const HttpRequest& request, HttpResponse& response)
            {
                jclass urlClass = env->FindClass("java/net/URL");
                jclass connectionClass = env->FindClass("java/net/HttpURLConnection");

                if (failed(env, response))
                {
                    return;
                }

                jmethodID urlInit = env->GetMethodID(urlClass, "<init>", "(Ljava/lang/String;)V");
                jmethodID openConnection = env->GetMethodID(urlClass, "openConnection", "()Ljava/net/URLConnection;");
                jmethodID setRequestMethod = env->GetMethodID(connectionClass, "setRequestMethod", "(Ljava/lang/String;)V");
                jmethodID setConnectTimeout = env->GetMethodID(connectionClass, "setConnectTimeout", "(I)V");
                jmethodID setReadTimeout = env->GetMethodID(connectionClass, "setReadTimeout", "(I)V");
                jmethodID setFollowRedirects = env->GetMethodID(connectionClass, "setInstanceFollowRedirects", "(Z)V");
                jmethodID setUseCaches = env->GetMethodID(connectionClass, "setUseCaches", "(Z)V");
                jmethodID setRequestProperty = env->GetMethodID(connectionClass, "setRequestProperty", "(Ljava/lang/String;Ljava/lang/String;)V");
                jmethodID setDoOutput = env->GetMethodID(connectionClass, "setDoOutput", "(Z)V");
                jmethodID setFixedLength = env->GetMethodID(connectionClass, "setFixedLengthStreamingMode", "(I)V");
                jmethodID getOutputStream = env->GetMethodID(connectionClass, "getOutputStream", "()Ljava/io/OutputStream;");
                jmethodID getResponseCode = env->GetMethodID(connectionClass, "getResponseCode", "()I");
                jmethodID getInputStream = env->GetMethodID(connectionClass, "getInputStream", "()Ljava/io/InputStream;");
                jmethodID getErrorStream = env->GetMethodID(connectionClass, "getErrorStream", "()Ljava/io/InputStream;");
                jmethodID getHeaderField = env->GetMethodID(connectionClass, "getHeaderField", "(Ljava/lang/String;)Ljava/lang/String;");
                jmethodID disconnect = env->GetMethodID(connectionClass, "disconnect", "()V");

                if (failed(env, response))
                {
                    return;
                }

                jobject url = env->NewObject(urlClass, urlInit, env->NewStringUTF(request.url.c_str()));

                if (failed(env, response))
                {
                    return;
                }

                jobject connection = env->CallObjectMethod(url, openConnection);

                if (failed(env, response) || !connection)
                {
                    return;
                }

                const jint timeoutMs = static_cast<jint>(request.timeoutSeconds) * 1000;

                env->CallVoidMethod(connection, setRequestMethod, env->NewStringUTF(request.method.c_str()));
                env->CallVoidMethod(connection, setConnectTimeout, static_cast<jint>(10000));
                env->CallVoidMethod(connection, setReadTimeout, timeoutMs);
                env->CallVoidMethod(connection, setFollowRedirects, JNI_FALSE);
                env->CallVoidMethod(connection, setUseCaches, JNI_FALSE);

                if (failed(env, response))
                {
                    return;
                }

                for (const auto& [name, value] : request.headers)
                {
                    jstring jName = env->NewStringUTF(name.c_str());
                    jstring jValue = env->NewStringUTF(value.c_str());

                    env->CallVoidMethod(connection, setRequestProperty, jName, jValue);
                    env->DeleteLocalRef(jName);
                    env->DeleteLocalRef(jValue);

                    if (failed(env, response))
                    {
                        return;
                    }
                }

                const bool sendsBody = !request.body.empty() ||
                    request.method == "POST" || request.method == "PUT";

                if (sendsBody)
                {
                    const auto length = static_cast<jint>(request.body.size());

                    env->CallVoidMethod(connection, setDoOutput, JNI_TRUE);
                    env->CallVoidMethod(connection, setFixedLength, length);

                    jobject output = env->CallObjectMethod(connection, getOutputStream);

                    if (failed(env, response) || !output)
                    {
                        env->CallVoidMethod(connection, disconnect);
                        env->ExceptionClear();
                        return;
                    }

                    jclass outputClass = env->FindClass("java/io/OutputStream");
                    jmethodID write = env->GetMethodID(outputClass, "write", "([B)V");
                    jmethodID close = env->GetMethodID(outputClass, "close", "()V");

                    jbyteArray bytes = env->NewByteArray(length);
                    env->SetByteArrayRegion(bytes, 0, length, reinterpret_cast<const jbyte*>(request.body.data()));
                    env->CallVoidMethod(output, write, bytes);
                    env->CallVoidMethod(output, close);

                    if (failed(env, response))
                    {
                        env->CallVoidMethod(connection, disconnect);
                        env->ExceptionClear();
                        return;
                    }
                }

                const jint status = env->CallIntMethod(connection, getResponseCode);

                if (failed(env, response))
                {
                    env->CallVoidMethod(connection, disconnect);
                    env->ExceptionClear();
                    return;
                }

                response.statusCode = status;

                jobject stream = status >= 400
                    ? env->CallObjectMethod(connection, getErrorStream)
                    : env->CallObjectMethod(connection, getInputStream);

                if (env->ExceptionCheck())
                {
                    env->ExceptionClear();
                    stream = nullptr;
                }

                response.body = readStream(env, stream);

                auto retryAfter = static_cast<jstring>(
                    env->CallObjectMethod(connection, getHeaderField, env->NewStringUTF("Retry-After"))
                );

                if (env->ExceptionCheck())
                {
                    env->ExceptionClear();
                    retryAfter = nullptr;
                }

                if (retryAfter)
                {
                    const char* chars = env->GetStringUTFChars(retryAfter, nullptr);

                    if (chars)
                    {
                        try
                        {
                            response.retryAfterSeconds = std::stoi(chars);
                        }
                        catch (...)
                        {
                        }

                        env->ReleaseStringUTFChars(retryAfter, chars);
                    }
                }

                env->CallVoidMethod(connection, disconnect);

                if (env->ExceptionCheck())
                {
                    env->ExceptionClear();
                }
            }
        };
    }

    std::unique_ptr<HttpClient> createDefaultHttpClient()
    {
        return std::make_unique<AndroidHttpClient>();
    }
}
