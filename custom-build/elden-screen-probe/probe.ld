ENTRY(_start)
SECTIONS {
  . = 0;
  .text : { KEEP(*(.text.entry)) *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  . = ALIGN(16);
  .probe_stack : { *(.probe_stack) }
  . = ALIGN(16);
  _image_end = .;
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
