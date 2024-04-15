#pragma once

#include "noncopyable.h"
#include <string>
#include "Timestamp.h"

// 定义日志级别 INFO ERROR  FATAL DEBUG

// LOG_INFO("%s %d", arg1, atg2)
// 宏有多行代码的话我们用do while(0) ##__VA_ARGS获取可变参数列表宏 \表示换行，写宏都要加，且\后面不能加空格 debug输出信息多，我们加上mudebug才会输出，不加logdebug为空
#define LOG_INFO(logmsgFormat, ...)                       \
    do                                                    \
    {                                                     \
        Logger &logger = Logger::instance();              \
        logger.setLogLevel(INFO);                         \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
    } while (0)

#define LOG_ERROR(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(ERROR); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
    } while(0) 
    
#define LOG_FATAL(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(FATAL); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
        exit(-1); \
    } while(0) 

#ifdef MUDEBUG
#define LOG_DEBUG(logmsgFormat, ...)                   \
    do                                                 \
    {                                                  \
        Logger &logger = Logger::instance();           \
        Logger.setLogLevel(DEBUG);                     \
        char buf[1024] = {0};                          \
        sprintf(buf, 1024, logmsgFormat, ##__VA_ARGS); \
        logger.log(buf);                               \
    } while (0)
#else
#define LOG_DEBUG(logmsgFormat, ...)
#endif
    enum LogLevel
    {
        INFO,  // 普通信息
        ERROR, // 错误信息
        FATAL, // core信息
        DEBUG, // 调试信息
    };

// 输出一个日志类
class Logger : noncopyable
{
public:
    // 获取日志唯一的实例对象
    static Logger &instance();
    // 设置日志级别
    void setLogLevel(int level);
    // 写日志
    void log(std::string msg);

private:
    // 成员变量后边加下横杠，有助于区分局部变量
    // 系统的是下横杠开头的，我们放后面有助于区分
    int logLevel_;
    Logger() {}
};