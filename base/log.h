#pragma once

#include <sstream>

#include "log_callback.h"

#if defined(ANDROID)
#include <android/log.h>
#else
#include <ctime>
#include <iostream>
#include <sstream>
#endif

#if !defined(WINDOWS)
// Remove path and extract only filename.
#define FILENAME \
    (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)
#else
#define FILENAME __FILE__
#endif

namespace base {

#define call_user_callback(...) call_user_callback_located(FILENAME, __LINE__, __VA_ARGS__)

#define LogDebug() LogDebugDetailed(FILENAME, __LINE__)
#define LogInfo() LogInfoDetailed(FILENAME, __LINE__)
#define LogWarn() LogWarnDetailed(FILENAME, __LINE__)
#define LogError() LogErrDetailed(FILENAME, __LINE__)

enum class Color {
    Red,
    Green,
    Yellow,
    Blue,
    Gray,
    Reset
};

void set_color(Color color);

class LogDetailed {
public:
    LogDetailed(const char *filename, int filenumber)
        : _s(), _caller_filename(filename), _caller_filenumber(filenumber) {}

    template <typename T>
    LogDetailed &operator<<(const T &x) {
        _s << x;
        return *this;
    }

    virtual ~LogDetailed() {
        if (log::get_callback() &&
            log::get_callback()(_log_level, _s.str(), _caller_filename, _caller_filenumber)) {
            return;
        }

        // Time output taken from:
        // https://stackoverflow.com/questions/16357999#answer-16358264
        time_t rawtime;
        time(&rawtime);
        struct tm *timeinfo = localtime(&rawtime);
        char time_buffer[10]{};  // We need 8 characters + \0
        strftime(time_buffer, sizeof(time_buffer), "%I:%M:%S", timeinfo);
        std::stringstream string_stream;
        string_stream << "[" << time_buffer;

        switch (_log_level) {
            case log::Level::Debug:
                string_stream << "|Debug] ";
                break;
            case log::Level::Info:
                string_stream << "|Info] ";
                break;
            case log::Level::Warn:
                string_stream << "|Warn] ";
                break;
            case log::Level::Err:
                string_stream << "|Error] ";
                break;
        }

        string_stream << _s.str();
        string_stream << " (" << _caller_filename << ":" << std::dec << _caller_filenumber << ")";

        printf("%s\n", string_stream.str().c_str());
    }

    LogDetailed(const base::LogDetailed &) = delete;
    void operator=(const base::LogDetailed &) = delete;
protected:
    log::Level _log_level = log::Level::Debug;
private:
    std::stringstream _s;
    const char *_caller_filename;
    int _caller_filenumber;
};

class LogDebugDetailed : public LogDetailed {
public:
    LogDebugDetailed(const char *filename, int filenumber) : LogDetailed(filename, filenumber) {
        _log_level = log::Level::Debug;
    }
};

class LogInfoDetailed : public LogDetailed {
public:
    LogInfoDetailed(const char *filename, int filenumber) : LogDetailed(filename, filenumber) {
        _log_level = log::Level::Info;
    }
};

class LogWarnDetailed : public LogDetailed {
public:
    LogWarnDetailed(const char *filename, int filenumber) : LogDetailed(filename, filenumber) {
        _log_level = log::Level::Warn;
    }
};

class LogErrDetailed : public LogDetailed {
public:
    LogErrDetailed(const char *filename, int filenumber) : LogDetailed(filename, filenumber) {
        _log_level = log::Level::Err;
    }
};

}  // namespace base
