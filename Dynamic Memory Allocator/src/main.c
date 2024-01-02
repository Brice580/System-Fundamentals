#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {
    

    char* x = sf_memalign(1003, 64);

    sf_show_heap();
    
    printf("%p",x );

    return EXIT_SUCCESS;
}
