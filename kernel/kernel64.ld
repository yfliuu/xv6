/* Simple linker script for the JOS kernel.
   See the GNU ld 'info' manual ("info ld") to learn the syntax. */

/* OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386") */
OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

mboot_load_addr = 0x00100000;

SECTIONS
{
	/* Link the kernel at this address: "." means the current address */
        /* Must be equal to KERNLINK */
	. = 0xFFFFFFFF80100000; 

	PROVIDE(begin = .);

	/* .text : AT(mboot_load_addr) {
		*(.text .rela.text .stub .text.* .gnu.linkonce.t.*)
	} */

	.text : AT(mboot_load_addr) {
		*(.text .rela.text .stub .gnu.linkonce.t.*)
		. = ALIGN(0x1000);
		*(.text.cross)
		*(.text.*)
	}

	. = ALIGN(0x1000);

	PROVIDE(etext = .);	/* Define the 'etext' symbol to this value */

	.rodata : {
		*(.rodata .rodata.* .gnu.linkonce.r.*)
	}

	/* Adjust the address for the data segment to the next page */
	. = ALIGN(0x1000);

	/* Conventionally, Unix linkers provide pseudo-symbols
	 * etext, edata, and end, at the end of the text, data, and bss.
	 * For the kernel mapping, we need the address at the beginning
	 * of the data section, but that's not one of the conventional
	 * symbols, because the convention started before there was a
	 * read-only rodata section between text and data. */
	PROVIDE(data = .);

	/* The data segment */
	.data : {
		*(.data)
	}

	. = ALIGN(0x1000);

	PROVIDE(edata = .);

	.bss : {
		*(.bss)
		*(COMMON)
	}

	. = ALIGN(0x1000);

	PROVIDE(spml4_root = .);

	. = . + 0x1000;

	/* Hopefully we'll align to 0xffffffff80200000 
	 * We'll use physical 0x200000 as the start of shared memory
	 */
	. = ALIGN(0x100000);

	PROVIDE(shared_memory_start = .);

	. = . + 0x10000;

	. = ALIGN(0x1000);

	PROVIDE(end = .);

	/DISCARD/ : {
		*(.eh_frame .rela.eh_frame .note.GNU-stack)
	}
}

mboot_load_end = mboot_load_addr + (edata - begin);
mboot_bss_end = mboot_load_addr + (end - begin);
mboot_entry_addr = mboot_load_addr + (mboot_entry - begin);
