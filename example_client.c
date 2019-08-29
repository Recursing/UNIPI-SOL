#include "access_library.h"
#include <stdio.h>
int main(int argc, char *argv[])
{
    printf("starting client\n");
    os_connect(argv[1]);

    os_store(argv[1], "\n1\n2\n3\n4\n5\n6\n7\n", 16);
    printf("os_connect returned\n");
    return 0;
}