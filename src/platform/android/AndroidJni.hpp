#pragma once

#include <jni.h>

namespace blitzdj::jni
{
    JavaVM* javaVM();

    void setJavaVM(JavaVM* vm);
}
