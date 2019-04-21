#pragma once
#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
#include <cstdio>
#include <cstdlib>
#include <sys/types.h>
#include <sys/wait.h>
#include <type_traits>
#include <unistd.h>
#include <utility>
// copied from cppreference as possible implementation
namespace detail {
template <class F, class... Args> inline auto INVOKE(F &&f, Args &&... args) -> decltype(std::forward<F>(f)(std::forward<Args>(args)...))
{
    return std::forward<F>(f)(std::forward<Args>(args)...);
}

template <class Base, class T, class Derived> inline auto INVOKE(T Base::*pmd, Derived &&ref) -> decltype(std::forward<Derived>(ref).*pmd)
{
    return std::forward<Derived>(ref).*pmd;
}

template <class PMD, class Pointer> inline auto INVOKE(PMD pmd, Pointer &&ptr) -> decltype((*std::forward<Pointer>(ptr)).*pmd)
{
    return (*std::forward<Pointer>(ptr)).*pmd;
}

template <class Base, class T, class Derived, class... Args>
inline auto INVOKE(T Base::*pmf, Derived &&ref, Args &&... args) -> decltype((std::forward<Derived>(ref).*pmf)(std::forward<Args>(args)...))
{
    return (std::forward<Derived>(ref).*pmf)(std::forward<Args>(args)...);
}

template <class PMF, class Pointer, class... Args>
inline auto INVOKE(PMF pmf, Pointer &&ptr, Args &&... args) -> decltype(((*std::forward<Pointer>(ptr)).*pmf)(std::forward<Args>(args)...))
{
    return ((*std::forward<Pointer>(ptr)).*pmf)(std::forward<Args>(args)...);
}
} // namespace detail

namespace util {
template <class F, class... ArgTypes> decltype(auto) invoke(F &&f, ArgTypes &&... args)
{
    return detail::INVOKE(std::forward<F>(f), std::forward<ArgTypes>(args)...);
}
} // namespace util

void Disable_Console_Output();

template <class F, class... Args> bool ABORTS(F &&f, Args &&... args)
{
    // pipe
    int fd[2];
    pipe(fd);

    // spawn a new process
    auto child_pid = fork();
    bool aborted = false;

    // if the fork succeeded
    if (child_pid >= 0) {

        // if we are in the child process
        if (child_pid == 0) {
            close(fd[0]);

            // call the function that we expect to abort
            Disable_Console_Output();
            util::invoke(std::forward<F>(f), std::forward<Args>(args)...);

            char succ[] = "1";
            write(fd[1], succ, 2);

            // if the function didn't abort, we'll exit cleanly
            std::exit(EXIT_SUCCESS);
        } else {
            close(fd[1]);
            char buff[128];
            int n = read(fd[0], buff, 127);
            aborted = n == 0;
        }
    }

    return aborted;
}
#else
template <class F, class... Args> bool ABORTS(F &&, Args &&...)
{
    return true;
}
#endif
