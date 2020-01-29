#include "types.h"
#include "user.h"

// A stub process for cross call
int main(int argc, char **argv)
{
    for(;;) {
        sleep(1000);
    }
    printf(1, "Error: stub exiting\n");
    return -1;
}