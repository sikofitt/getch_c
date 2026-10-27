#include <stdio.h>
#include "../getch.h"

/* Links against whichever library the build gives it; exercises one pushback round trip. */
int main(void)
{
    if(_ungetch('a') != 'a') {
        return 1;
    }

    return cinPeek() == 'a' && _getch() == 'a' ? 0 : 1;
}
