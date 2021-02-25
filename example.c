#include <stdio.h>
#include "getch.h"

int main() {

   int key = _getch();
    
    printf("Key %d\n", key);

   if (key == 0 || key == 224) {
        key = _getch();
        printf("Special Key : %d\n", key);
    }
}