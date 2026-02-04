#define _CRT_SECURE_NO_WARNINGS // 防止 VS 报错 localtime 不安全

#include "Utils.h" // 对应上面的头文件
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <stdarg.h>

// --- Windows/Linux 跨平台宏 ---
#ifdef _WIN32
#include <windows.h>
#include <share.h>
#include <direct.h>
#define PATH_SEPARATOR "\\"

// Windows 锁
static CRITICAL_SECTION g_log_mutex;
static int g_mutex_initialized = 0;

static void mutex_init() {
    if (!g_mutex_initialized) {
        InitializeCriticalSection(&g_log_mutex);
        g_mutex_initialized = 1;
    }
}

static void mutex_lock() {
    if (g_mutex_initialized) EnterCriticalSection(&g_log_mutex);
}

static void mutex_unlock() {
    if (g_mutex_initialized) LeaveCriticalSection(&g_log_mutex);
}

static void mutex_destroy() {
    if (g_mutex_initialized) {
        DeleteCriticalSection(&g_log_mutex);
        g_mutex_initialized = 0;
    }
}

static void make_dir(const char* dir) { _mkdir(dir); }
static void get_local_time(struct tm* t, time_t* timep) { localtime_s(t, timep); }

#else
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#define PATH_SEPARATOR "/"

// Linux 锁
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;
static void mutex_init() {}
static void mutex_lock() { pthread_mutex_lock(&g_log_mutex); }
static void mutex_unlock() { pthread_mutex_unlock(&g_log_mutex); }
static void mutex_destroy() {}
static void make_dir(const char* dir) { mkdir(dir, 0755); }
static void get_local_time(struct tm* t, time_t* timep) { localtime_r(timep, t); }
#endif

// --- 变量定义 ---
static const char* FILE_NAMES[] = { "debug.log", "info.log", "warning.log", "error.log" };
static const char* COLOR_RESET = "\033[0m";
static const char* COLOR_RED = "\033[91m";
static const char* COLOR_GREEN = "\033[92m";
static const char* COLOR_YELLOW = "\033[93m";
static const char* COLOR_BLUE = "\033[94m";

static char g_log_dir[256] = "logs";
static int g_enable_file = 1;
static int g_console_levels[4] = { 1, 1, 1, 1 };

// --- 内部实现 ---

static void ensure_dir_exists() {
    make_dir(g_log_dir);
}

static void get_time_str(char* buffer, size_t size) {
    time_t rawtime;
    struct tm timeinfo;
    time(&rawtime);
    get_local_time(&timeinfo, &rawtime);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", &timeinfo);
}

static void get_level_info(LogLevel level, const char** str, const char** color) {
    switch (level) {
    case LOG_LEVEL_INFO:    *str = "INFO";    *color = COLOR_GREEN; break;
    case LOG_LEVEL_WARNING: *str = "WARNING"; *color = COLOR_YELLOW; break;
    case LOG_LEVEL_ERROR:   *str = "ERROR";   *color = COLOR_RED; break;
    case LOG_LEVEL_DEBUG:   *str = "DEBUG";   *color = COLOR_BLUE; break;
    default:                *str = "UNKNOWN"; *color = COLOR_RESET; break;
    }
}

// --- 接口实现 (匹配你原来的 C++ 接口名) ---

void initLogger(void) {
    mutex_init();
    mutex_lock();
    ensure_dir_exists();
    mutex_unlock();
}

void closeLogger(void) {
    mutex_destroy();
}

void enableFileLogging(int enable) {
    mutex_lock();
    g_enable_file = enable;
    mutex_unlock();
}

void setLogDirectory(const char* dir) {
    mutex_lock();
    if (dir && strlen(dir) < 255) {
        strcpy(g_log_dir, dir);
        ensure_dir_exists();
    }
    mutex_unlock();
}

void setConsoleOutputLevels(int debug, int info, int warning, int error) {
    mutex_lock();
    g_console_levels[0] = debug;
    g_console_levels[1] = info;
    g_console_levels[2] = warning;
    g_console_levels[3] = error;
    mutex_unlock();
}

// 这就是报错说找不到的那个函数！
void logMessage(LogLevel level, const char* format, ...) {
    if (level < 0 || level > 3) return;

    char time_buf[32];
    const char* level_str;
    const char* color;
    char msg_buf[4096]; // 增加缓冲区以防溢出
    char file_path[512];

    va_list args;
    va_start(args, format);
    vsnprintf(msg_buf, sizeof(msg_buf), format, args);
    va_end(args);

    get_time_str(time_buf, sizeof(time_buf));
    get_level_info(level, &level_str, &color);

    mutex_lock();

    // 1. 控制台输出
    if (g_console_levels[level]) {
        printf("%s[%s][%s] %s%s\n", color, level_str, time_buf, msg_buf, COLOR_RESET);
    }

    // 2. 文件输出
    if (g_enable_file) {
        snprintf(file_path, sizeof(file_path), "%s%s%s", g_log_dir, PATH_SEPARATOR, FILE_NAMES[level]);

        FILE* fp = NULL;
#ifdef _WIN32
        // 使用 _fsopen 允许共享读 (_SH_DENYNO)
        fp = _fsopen(file_path, "a", _SH_DENYNO);
#else
        fp = fopen(file_path, "a");
#endif

        if (fp) {
            fprintf(fp, "[%s][%s] %s\n", time_buf, level_str, msg_buf);
            fflush(fp);
            fclose(fp);
        }
    }

    mutex_unlock();
}