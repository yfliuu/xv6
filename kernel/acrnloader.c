// Boot loader.
//
// Part of the boot sector, along with bootasm.S, which calls bootmain().
// bootasm.S has put the processor into protected 32-bit mode.
// bootmain() loads a multiboot kernel image from the disk starting at
// sector 1 and then jumps to the kernel entry routine.

typedef unsigned int uint32_t;
typedef unsigned char uint8_t;

extern char _binary_kernel_bin_start[];
extern char _binary_kernel_bin_size[];

struct mbheader {
    uint32_t magic;
    uint32_t flags;
    uint32_t checksum;
    uint32_t header_addr;
    uint32_t load_addr;
    uint32_t load_end_addr;
    uint32_t bss_end_addr;
    uint32_t entry_addr;
};

void memcpy(uint8_t *dst, const uint8_t *src, uint32_t size) {
    int i;
    for (i = 0; i < size; i++)
        dst[i] = src[i];
}

int start(void) {
    struct mbheader *hdr;
    void (*entry)(void);
    int found = 0;
    uint8_t x[8192];
    uint32_t n;

    // multiboot header must be in the first 8192 bytes
    memcpy(x, _binary_kernel_bin_start, 8192);

    for (n = 0; n < 8192 / 4; n++)
        if (x[n] == 0x1BADB002)
            if ((x[n] + x[n + 1] + x[n + 2]) == 0) {
                found = 1;
                break;
            }

    if (!found) return -1;

    hdr = (struct mbheader *)(x + n);

    if (!(hdr->flags & 0x10000))
        return -1;  // does not have load_* fields, cannot proceed
    if (hdr->load_addr > hdr->header_addr)
        return -1;  // invalid;
    if (hdr->load_end_addr < hdr->load_addr)
        return -1;  // no idea how much to load

    memcpy((uint8_t *)hdr->load_addr, _binary_kernel_bin_start, (uint32_t)_binary_kernel_bin_size);

    // Call the entry point from the multiboot header.
    // Does not return!
    entry = (void (*)(void))(hdr->entry_addr);
    entry();

    return 0;
}