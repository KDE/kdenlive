#include "abortutil.hpp"
#ifdef __linux__

void Disable_Console_Output()
{
    // close C file descriptors
    fclose(stdout);
    fclose(stderr);
}

#endif
