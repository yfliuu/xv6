OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)

SECTIONS
{
    . = 4M + SIZEOF_HEADERS;
    stext = .;
    .text : { *(.init) *(.text) *(.text.*) }
    . = ALIGN(4K);
    .data : {
          *(.data)
          exception_table_start = .;
          *(.data.ex)
	  exception_table_end = .;
	  }
    . = ALIGN(16);
    .rodata : { *(.rodata) }
    . = ALIGN(16);
    .stack : { *(stack) }
    . = ALIGN(16);
    . = . + 4096;
    PROVIDE(stacktop = .);
    . = . + 16;
    . = ALIGN(16);
    bss_start = .;
    .bss : { *(.bss) }
    . = ALIGN(4K);
    edata = .;

    /DISCARD/ : {
        *(.comment .comment.* .note .note.*)
    }
}

ENTRY(start)
