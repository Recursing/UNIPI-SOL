#include "access_library.h"
#include <stdio.h>
int main()
{
    printf("starting client\n");
    os_connect("pizza");

    os_store("pasta", "\n1\n2\n3\n4\n5\n6\n7\n", 15);
    printf("os_connect returned\n");
    return 0;
}