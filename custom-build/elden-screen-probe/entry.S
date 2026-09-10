.section .text.entry,"ax"
.global _start
_start:
  b probe_entry
  nop
  .quad 0x80000
  .quad _image_end
  .quad 0
  .quad 0
  .quad 0
  .quad 0
  .ascii "ARM\x64"
  .long 0
probe_entry:
  // Preserve ABL's x0 (FDT). Only CPU interrupt masks and our own SP change.
  msr daifset, #15
  adr x1, _stack_end
  mov sp, x1
  // Refuse physical framebuffer access unless ABL disabled MMU and D-cache.
  mrs x2, CurrentEL
  cmp x2, #4
  b.eq 1f
  cmp x2, #8
  b.ne probe_stop
  mrs x2, sctlr_el2
  b 2f
1:
  mrs x2, sctlr_el1
2:
  tst x2, #1
  b.ne probe_stop
  tst x2, #4
  b.ne probe_stop
  bl probe_main
probe_stop:
  // Deliberate stop, not Android/Aloha. Use the user's tested key recovery.
  wfe
  b probe_stop

.section .probe_stack,"aw"
.balign 16
_stack_begin:
  .space 8192
_stack_end:
