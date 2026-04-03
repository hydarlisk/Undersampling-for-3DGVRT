#pragma once

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "vulkanProject"
#else
#include <stdio.h>
#endif

#ifdef __ANDROID__
#define LOGV(...) ((void)__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__))
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO,    LOG_TAG, __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN,    LOG_TAG, __VA_ARGS__))
#define LOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG,   LOG_TAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR,   LOG_TAG, __VA_ARGS__))
#else
#define LOGV(...) ((void)(printf(__VA_ARGS__)))
#define LOGI(...) ((void)(printf(__VA_ARGS__)))
#define LOGW(...) ((void)(printf(__VA_ARGS__)))
#define LOGD(...) ((void)(printf(__VA_ARGS__)))
#define LOGE(...) ((void)(fprintf(stderr, "[E][%s] ", "vulkan"), fprintf(stderr, __VA_ARGS__), fprintf(stderr, "\n")))
//#define LOGV(...) ((void)(printf(__VA_ARGS__), printf("\n")))
//#define LOGI(...) ((void)(printf(__VA_ARGS__), printf("\n")))
//#define LOGW(...) ((void)(printf(__VA_ARGS__), printf("\n")))
//#define LOGD(...) ((void)(printf(__VA_ARGS__), printf("\n")))
//#define LOGE(...) ((void)(fprintf(stderr, "[E][%s] ", "vulkan"), fprintf(stderr, __VA_ARGS__), fprintf(stderr, "\n")))
#endif