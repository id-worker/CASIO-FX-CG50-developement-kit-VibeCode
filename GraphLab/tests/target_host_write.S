/* Linux-user write syscall, for synthetic QEMU testing only. */
.section .text
.global _host_write
_host_write:
  mov #4,r3
  trapa #0x10
  rts
  nop
