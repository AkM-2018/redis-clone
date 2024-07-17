#ifndef PRINT_UTILS_H
#define PRINT_UTILS_H

#include <iostream>

template <typename T>
void print(T &&t)
{
    std::cout << t << "\n";
}

template <typename T, typename... Args>
void print(T &&t, Args &&...args)
{
    std::cout << t << " ";
    print(std::forward<Args>(args)...);
}

static void msg(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
}

static void die(const char *msg)
{
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

#endif