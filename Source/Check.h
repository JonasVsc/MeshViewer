#pragma once
#include <exception>
#include <print>

#define FATAL_CHECK(condition, fmt, ...) \
    do { \
        if (!(condition)) { \
            std::println(stderr, "[{}:{}] FATAL: " fmt, \
                         __FILE__, __LINE__, ##__VA_ARGS__); \
            std::terminate(); \
        } \
    } while (false)