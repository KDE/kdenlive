#include "abortutil.hpp"
#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))

void Disable_Console_Output()
{
    // close C file descriptors
    fclose(stdout);
    fclose(stderr);
}

#endif
