#pragma once
#include <android/log.h>
#include <cstdarg>
#include <cstdio>

#define LOG_TAG "OwenGameServer"

inline void LOGI(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    __android_log_vprint(ANDROID_LOG_INFO, LOG_TAG, fmt, ap);
    va_end(ap);
}

inline void LOGE(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    __android_log_vprint(ANDROID_LOG_ERROR, LOG_TAG, fmt, ap);
    va_end(ap);
}

inline void LOGW(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    __android_log_vprint(ANDROID_LOG_WARN, LOG_TAG, fmt, ap);
    va_end(ap);
}
