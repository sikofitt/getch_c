#include <stdio.h>
#include "getch.h"

int main() {

   int key = _getch();
    
    printf("Key %d\n", key);

   if (key == 0 || key == 224) {
        key = _getch();
        printf("Special Key : %d\n", key);
    }

   do {
       key = _getch();
       printf("Key : %d\n", key);
   } while(key != 113);

    _ungetch('A');

    int res  = cinPeek();
    int getch = _getch();

    printf("%c : %c\n\r", res, getch);

}