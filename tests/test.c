#define CTEST_MAIN

#include <stdio.h>
#include "ctest.h"

int main(int argc, const char *argv[])
{
    // The tests feed stdin themselves; never depend on the caller's stdin.
    if(freopen("/dev/null", "r", stdin) == NULL) {
        return 1;
    }

    if(argc == 1) {
        argv[1] = "getchTests";
        argc = 2;
    }
    int result = ctest_main(argc, argv);
    return result;
}