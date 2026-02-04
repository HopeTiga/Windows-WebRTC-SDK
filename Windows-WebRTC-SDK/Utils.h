#ifndef UTILS_H
#define UTILS_H

#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

    // 1. 保持和你原有代码一致的枚举
    typedef enum {
        LOG_LEVEL_DEBUG = 0,
        LOG_LEVEL_INFO,
        LOG_LEVEL_WARNING,
        LOG_LEVEL_ERROR
    } LogLevel;

    // 2. 保持和你原有代码一致的函数名
    void initLogger(void);
    void closeLogger(void);
    void enableFileLogging(int enable);
    void setLogDirectory(const char* dir);
    void setConsoleOutputLevels(int debug, int info, int warning, int error);

    // 核心函数：logMessage (这是报错提示缺少的符号)
    void logMessage(LogLevel level, const char* format, ...);

    // 辅助宏：为了兼容你旧代码中可能存在的宏调用
#define LOG_INFO(fmt, ...)    logMessage(LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) logMessage(LOG_LEVEL_WARNING, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)   logMessage(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)   logMessage(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // UTILS_H