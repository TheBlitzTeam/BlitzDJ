#include "AndroidJni.hpp"
#include "BlitzDJBridge.h"

#include <string>

namespace
{
    std::string fromJava(JNIEnv* env, jstring value)
    {
        if (!value)
        {
            return {};
        }

        jclass stringClass = env->FindClass("java/lang/String");
        jmethodID getBytes = env->GetMethodID(stringClass, "getBytes", "(Ljava/lang/String;)[B");
        jstring charset = env->NewStringUTF("UTF-8");
        auto bytes = static_cast<jbyteArray>(env->CallObjectMethod(value, getBytes, charset));

        std::string result;

        if (env->ExceptionCheck())
        {
            env->ExceptionClear();
        }
        else if (bytes)
        {
            const jsize length = env->GetArrayLength(bytes);
            result.resize(static_cast<std::size_t>(length));

            if (length > 0)
            {
                env->GetByteArrayRegion(bytes, 0, length, reinterpret_cast<jbyte*>(&result[0]));
            }

            env->DeleteLocalRef(bytes);
        }

        env->DeleteLocalRef(charset);
        env->DeleteLocalRef(stringClass);
        return result;
    }

    jstring utf8String(JNIEnv* env, const std::string& value)
    {
        jclass stringClass = env->FindClass("java/lang/String");
        jmethodID constructor = env->GetMethodID(stringClass, "<init>", "([BLjava/lang/String;)V");

        const auto length = static_cast<jsize>(value.size());
        jbyteArray bytes = env->NewByteArray(length);

        if (length > 0)
        {
            env->SetByteArrayRegion(bytes, 0, length, reinterpret_cast<const jbyte*>(value.data()));
        }

        jstring charset = env->NewStringUTF("UTF-8");
        auto result = static_cast<jstring>(env->NewObject(stringClass, constructor, bytes, charset));

        if (env->ExceptionCheck())
        {
            env->ExceptionClear();
            result = env->NewStringUTF("");
        }

        env->DeleteLocalRef(charset);
        env->DeleteLocalRef(bytes);
        env->DeleteLocalRef(stringClass);
        return result;
    }

    jstring toJava(JNIEnv* env, char* value, const char* fallback = "")
    {
        const std::string text = value ? value : fallback;
        blitzdj_free_string(value);
        return utf8String(env, text);
    }

    blitzdj_handle* handleOf(jlong handle)
    {
        return reinterpret_cast<blitzdj_handle*>(handle);
    }

    jboolean flag(int value)
    {
        return value != 0 ? JNI_TRUE : JNI_FALSE;
    }
}

#define BLITZDJ_JNI(name) Java_com_theblitzteam_blitzdj_NativeBridge_##name

extern "C"
{
    JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*)
    {
        blitzdj::jni::setJavaVM(vm);
        return JNI_VERSION_1_6;
    }

    JNIEXPORT jlong JNICALL BLITZDJ_JNI(create)(JNIEnv* env, jobject, jstring clientId, jstring redirectUri, jstring sessionPath)
    {
        const auto id = fromJava(env, clientId);
        const auto redirect = fromJava(env, redirectUri);
        const auto session = fromJava(env, sessionPath);

        return reinterpret_cast<jlong>(blitzdj_create(id.c_str(), redirect.c_str(), session.c_str()));
    }

    JNIEXPORT void JNICALL BLITZDJ_JNI(destroy)(JNIEnv*, jobject, jlong handle)
    {
        blitzdj_destroy(handleOf(handle));
    }

    JNIEXPORT jstring JNICALL BLITZDJ_JNI(version)(JNIEnv* env, jobject)
    {
        return utf8String(env, blitzdj_version());
    }

    JNIEXPORT jstring JNICALL BLITZDJ_JNI(authorizationUrl)(JNIEnv* env, jobject, jlong handle)
    {
        return toJava(env, blitzdj_authorization_url(handleOf(handle)));
    }

    JNIEXPORT jboolean JNICALL BLITZDJ_JNI(handleRedirect)(JNIEnv* env, jobject, jlong handle, jstring url)
    {
        const auto value = fromJava(env, url);
        return flag(blitzdj_handle_redirect(handleOf(handle), value.c_str()));
    }

    JNIEXPORT jboolean JNICALL BLITZDJ_JNI(isAuthenticated)(JNIEnv*, jobject, jlong handle)
    {
        return flag(blitzdj_is_authenticated(handleOf(handle)));
    }

    JNIEXPORT void JNICALL BLITZDJ_JNI(logout)(JNIEnv*, jobject, jlong handle)
    {
        blitzdj_logout(handleOf(handle));
    }

    JNIEXPORT jboolean JNICALL BLITZDJ_JNI(play)(JNIEnv*, jobject, jlong handle)
    {
        return flag(blitzdj_play(handleOf(handle)));
    }

    JNIEXPORT jboolean JNICALL BLITZDJ_JNI(pause)(JNIEnv*, jobject, jlong handle)
    {
        return flag(blitzdj_pause(handleOf(handle)));
    }

    JNIEXPORT jboolean JNICALL BLITZDJ_JNI(next)(JNIEnv*, jobject, jlong handle)
    {
        return flag(blitzdj_next(handleOf(handle)));
    }

    JNIEXPORT jboolean JNICALL BLITZDJ_JNI(previous)(JNIEnv*, jobject, jlong handle)
    {
        return flag(blitzdj_previous(handleOf(handle)));
    }

    JNIEXPORT jboolean JNICALL BLITZDJ_JNI(setVolume)(JNIEnv*, jobject, jlong handle, jint volume)
    {
        return flag(blitzdj_set_volume(handleOf(handle), volume));
    }

    JNIEXPORT jboolean JNICALL BLITZDJ_JNI(seek)(JNIEnv*, jobject, jlong handle, jint positionMs)
    {
        return flag(blitzdj_seek(handleOf(handle), positionMs));
    }

    JNIEXPORT jboolean JNICALL BLITZDJ_JNI(setShuffle)(JNIEnv*, jobject, jlong handle, jboolean enabled)
    {
        return flag(blitzdj_set_shuffle(handleOf(handle), enabled == JNI_TRUE ? 1 : 0));
    }

    JNIEXPORT jboolean JNICALL BLITZDJ_JNI(setRepeat)(JNIEnv* env, jobject, jlong handle, jstring mode)
    {
        const auto value = fromJava(env, mode);
        return flag(blitzdj_set_repeat(handleOf(handle), value.c_str()));
    }

    JNIEXPORT jstring JNICALL BLITZDJ_JNI(queueJson)(JNIEnv* env, jobject, jlong handle, jint limit)
    {
        return toJava(env, blitzdj_queue_json(handleOf(handle), limit), "[]");
    }

    JNIEXPORT jstring JNICALL BLITZDJ_JNI(playbackJson)(JNIEnv* env, jobject, jlong handle)
    {
        return toJava(env, blitzdj_playback_json(handleOf(handle)), "{\"active\":false}");
    }

    JNIEXPORT jstring JNICALL BLITZDJ_JNI(devicesJson)(JNIEnv* env, jobject, jlong handle)
    {
        return toJava(env, blitzdj_devices_json(handleOf(handle)), "[]");
    }

    JNIEXPORT jstring JNICALL BLITZDJ_JNI(searchJson)(JNIEnv* env, jobject, jlong handle, jstring query, jint limit)
    {
        const auto value = fromJava(env, query);
        return toJava(env, blitzdj_search_json(handleOf(handle), value.c_str(), limit), "[]");
    }

    JNIEXPORT jstring JNICALL BLITZDJ_JNI(recommendJson)(JNIEnv* env, jobject, jlong handle, jstring query, jint count)
    {
        const auto value = fromJava(env, query);
        return toJava(env, blitzdj_recommend_json(handleOf(handle), value.c_str(), count), "[]");
    }

    JNIEXPORT jboolean JNICALL BLITZDJ_JNI(playTrack)(JNIEnv* env, jobject, jlong handle, jstring uri)
    {
        const auto value = fromJava(env, uri);
        return flag(blitzdj_play_track(handleOf(handle), value.c_str()));
    }

    JNIEXPORT jboolean JNICALL BLITZDJ_JNI(queueTrack)(JNIEnv* env, jobject, jlong handle, jstring uri)
    {
        const auto value = fromJava(env, uri);
        return flag(blitzdj_queue_track(handleOf(handle), value.c_str()));
    }

    JNIEXPORT jboolean JNICALL BLITZDJ_JNI(transferPlayback)(JNIEnv* env, jobject, jlong handle, jstring deviceId)
    {
        const auto value = fromJava(env, deviceId);
        return flag(blitzdj_transfer_playback(handleOf(handle), value.c_str()));
    }

    JNIEXPORT jboolean JNICALL BLITZDJ_JNI(startAutoDJ)(JNIEnv*, jobject, jlong handle)
    {
        return flag(blitzdj_start_auto_dj(handleOf(handle)));
    }

    JNIEXPORT void JNICALL BLITZDJ_JNI(stopAutoDJ)(JNIEnv*, jobject, jlong handle)
    {
        blitzdj_stop_auto_dj(handleOf(handle));
    }

    JNIEXPORT jboolean JNICALL BLITZDJ_JNI(isAutoDJRunning)(JNIEnv*, jobject, jlong handle)
    {
        return flag(blitzdj_is_auto_dj_running(handleOf(handle)));
    }

    JNIEXPORT void JNICALL BLITZDJ_JNI(setSearchQuery)(JNIEnv* env, jobject, jlong handle, jstring query)
    {
        const auto value = fromJava(env, query);
        blitzdj_set_search_query(handleOf(handle), value.c_str());
    }

    JNIEXPORT jstring JNICALL BLITZDJ_JNI(searchQuery)(JNIEnv* env, jobject, jlong handle)
    {
        return toJava(env, blitzdj_search_query(handleOf(handle)));
    }

    JNIEXPORT jboolean JNICALL BLITZDJ_JNI(queueRecommendations)(JNIEnv*, jobject, jlong handle, jint count)
    {
        return flag(blitzdj_queue_recommendations(handleOf(handle), count));
    }

    JNIEXPORT void JNICALL BLITZDJ_JNI(addFavoriteArtist)(JNIEnv* env, jobject, jlong handle, jstring artist)
    {
        const auto value = fromJava(env, artist);
        blitzdj_add_favorite_artist(handleOf(handle), value.c_str());
    }

    JNIEXPORT void JNICALL BLITZDJ_JNI(addFavoriteTrack)(JNIEnv* env, jobject, jlong handle, jstring trackId)
    {
        const auto value = fromJava(env, trackId);
        blitzdj_add_favorite_track(handleOf(handle), value.c_str());
    }

    JNIEXPORT jstring JNICALL BLITZDJ_JNI(lastError)(JNIEnv* env, jobject, jlong handle)
    {
        return toJava(env, blitzdj_last_error(handleOf(handle)));
    }
}
