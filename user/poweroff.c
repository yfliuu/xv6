#include "types.h"
#include "x86.h"
#include "user.h"

int
main(int argc, char *argv[])
{
    // To poweroff, add -device isa-debug-exit to qemu
    pwoff();
    // Make compiler happy
    return 0;
}
