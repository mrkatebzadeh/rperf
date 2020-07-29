/*
 * Copyright (c) 2012 David Rodrigues
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef __MACROLOGGER_H__
#define __MACROLOGGER_H__

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "progressbar.h"

static inline char *timenow();
extern FILE *log_fp;

#define _FILE strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__

#define NO_LOG 0x00
#define ERROR_LEVEL 0x01
#define INFO_LEVEL 0x02
#define DEBUG_LEVEL 0x03
#define VERBOSE_LEVEL 0x04

#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define BLUE "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN "\x1b[36m"
#define RESET "\x1b[0m"

#ifndef LOG_LEVEL
#define LOG_LEVEL INFO_LEVEL
#endif

#define PRINTFUNCTION(format, ...) fprintf(stderr, format, __VA_ARGS__)
#define LOGFUNCTION(format, ...) fprintf(log_fp, format, __VA_ARGS__)

#define LOG_FMT "%s %s%-5s%s %s:%s:%d|"
#define LOG_ARGS(LOG_TAG, COLOR) timenow(), COLOR, LOG_TAG, RESET, _FILE, __FUNCTION__, __LINE__
#define FILE_ARGS(LOG_TAG) timenow(), "", LOG_TAG, "", _FILE, __FUNCTION__, __LINE__

#define NEWLINE "\n"

#define ERROR_TAG "ERROR"
#define INFO_TAG "INFO"
#define DEBUG_TAG "DEBUG"
#define VERBOSE_TAG "VEBUG"

#define LOG_FILE(message, args...)                                        \
    {                                                                     \
        LOG_INFO(message, ##args);                                        \
        LOGFUNCTION(LOG_FMT message NEWLINE, FILE_ARGS(INFO_TAG), ##args); \
    }

#if LOG_LEVEL >= VERBOSE_LEVEL
#define LOG_VERBOSE(message, args...) \
    PRINTFUNCTION(LOG_FMT message NEWLINE, LOG_ARGS(VERBOSE_TAG, MAGENTA), ##args)
#else
#define LOG_VERBOSE(message, args...)
#endif

#if LOG_LEVEL >= DEBUG_LEVEL
#define LOG_DEBUG(message, args...) \
    PRINTFUNCTION(LOG_FMT message NEWLINE, LOG_ARGS(DEBUG_TAG, MAGENTA), ##args)
#else
#define LOG_DEBUG(message, args...)
#endif

#if LOG_LEVEL >= INFO_LEVEL
#define LOG_INFO(message, args...) \
    PRINTFUNCTION(LOG_FMT message NEWLINE, LOG_ARGS(INFO_TAG, GREEN), ##args)
#else
#define LOG_INFO(message, args...)
#endif

#if LOG_LEVEL >= ERROR_LEVEL
#define LOG_ERROR(message, args...) \
    PRINTFUNCTION(LOG_FMT message NEWLINE, LOG_ARGS(ERROR_TAG, RED), ##args)
#else
#define LOG_ERROR(message, args...)
#endif

#if LOG_LEVEL >= NO_LOGS
#define LOG_IF_ERROR(condition, message, args...) \
    if (condition)                                \
    PRINTFUNCTION(LOG_FMT message NEWLINE, LOG_ARGS(ERROR_TAG, RED), ##args)
#else
#define LOG_IF_ERROR(condition, message, args...)
#endif

#define check(A, M, ...)             \
    if (!(A)) {                      \
        LOG_ERROR(M, ##__VA_ARGS__); \
        errno = 0;                   \
        goto error;                  \
    }

static inline char *timenow() {
    static char buffer[64];
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer, 64, "%H:%M:%S", timeinfo);

    return buffer;
}

#endif
