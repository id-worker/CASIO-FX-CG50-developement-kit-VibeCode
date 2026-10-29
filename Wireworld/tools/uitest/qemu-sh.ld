/* Freestanding Linux/QEMU test image, not used by the calculator add-in. */
OUTPUT_FORMAT("elf32-sh")
ENTRY(_start)
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
SECTIONS {
 . = 0x10000;
 .text : { *(.text .text.*) *(.rodata .rodata.*) } :text
 . = ALIGN(0x1000);
 .data : { *(.data .data.*) } :data
 .bss : { *(.bss .bss.* COMMON) } :data
 /DISCARD/ : { *(.stack) *(.comment) *(.note*) }
}
