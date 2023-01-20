#pragma once

#ifndef UTIL_H
#define UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>

#define DEBUG
#define TA


uint32_t compute_checksum(const void* pkt, size_t n_bytes);

static inline uint64_t now_us () {
    //  Use POSIX gettimeofday function to get precise time.
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (tv.tv_sec * (uint64_t) 1000000 + tv.tv_usec);
}

static inline int msleep(unsigned int tms) {
    return usleep(tms * 1000);
}

#define LOG_ERROR(...) \
    do { \
        fprintf(stderr, "\033[40;31m[ERROR] \033[0m" __VA_ARGS__); \
        fflush(stderr); \
    } while (0)
#define LOG_MSG(...) \
    do { \
        fprintf(stdout, "\033[40;32m[INFO]  \033[0m" __VA_ARGS__); \
        fflush(stdout);\
    } while (0)
#ifdef DEBUG
#define LOG_DEBUG(...) \
    do { \
        fprintf(stderr, "\033[40;31m[DEBUG] \033[0m" __VA_ARGS__); \
        fflush(stderr); \
    } while (0)
#else
#define LOG_DEBUG(...)
#endif

#ifdef __cplusplus
}
#endif

#endif