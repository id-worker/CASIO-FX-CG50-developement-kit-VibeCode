/* Minimal Linux-user SuperH entry, for QEMU tests only, not in the add-in. */
.section .text.entry,"ax"
.global _start
_start:
  mov.l 1f,r15       /* simulate the calculator's 16 KiB stack */
  mov.l 2f,r1
  jsr @r1
  nop
  mov r0,r4
  mov #1,r3          /* Linux exit syscall */
  trapa #0x10
.align 2
1: .long _target_stack+16384
2: .long _main
