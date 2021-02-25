#define CTEST_MAIN

#include "ctest.h"

int main(int argc, const char *argv[])
{
    if(argc == 1) {
        argv[1] = "getchTests";
        argc = 2;
    }
    int result = ctest_main(argc, argv);
    return result;
}