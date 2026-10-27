#include <cstdio>
#include "../getch.h"

// Fails to link if getch.h lacks extern "C" guards.
int main()
{
    if(_ungetch('a') != 'a') {
        return 1;
    }

    return cinPeek() == 'a' && _getch() == 'a' ? 0 : 1;
}
