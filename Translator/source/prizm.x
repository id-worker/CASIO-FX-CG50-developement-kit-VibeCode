OUTPUT_FORMAT(binary)
OUTPUT_ARCH(sh3)

ENTRY(initialize)

MEMORY
{
        rom (rx)  : o = 0x00300000, l = 1024k
        ram (rwx) : o = 0x08100004, l = 128k
        ilram (rwx) : o = 0xE5200000, l = 4k
}

SECTIONS
{
        .text : {
                *(.pretext)
                *(.text)
                *(.text.*)
                . = ALIGN(4);
        } > rom

        .ilram : ALIGN(4) {
                _ilramld = LOADADDR(.ilram) ;
                _silram = . ;
                *(.ilram)
                *(.ilram.*)
                . = ALIGN(4);
                _eilram = . ;
        } > ilram AT> rom

        .rodata : ALIGN(4) {
                *(.rodata)
                *(.rodata.*)
                . = ALIGN(4);
        } > rom

        .data : ALIGN(4) {
                _datald = LOADADDR(.data) ;
                _sdata = . ;
                *(.data)
                *(.data.*)
                . = ALIGN(4);
                _edata = . ;
        } >ram AT>rom

        .bss : ALIGN(4) {
                _bbss = . ;
                *(.bss)
                *(.bss*)
                *(COMMON)
                . = ALIGN(4);
                _ebss = . ;
        } >ram
}
