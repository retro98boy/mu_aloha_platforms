/* Hash-pinned I2CDxe rejection-path recovery, linked at RVA 0xb800.
 * Only WRAP1 SE5, idle I2C/DMA; enabled GSI is refused after snapshot.
 * Reuse the stock firmware loader, including its clock/RAM sequencing.
 * Neither a PMIC operation nor a qupfw partition write is performed.
 */
.text
.global _start
_start:
    stp x19, x30, [sp, #-64]!
    str xzr, [sp, #24]
    str xzr, [sp, #32]
    str xzr, [sp, #40]
    mov w19, #1
    ldr x9, [x20, #0x18]
    mov x10, #0x4000
    movk x10, #0xa9, lsl #16
    cmp x9, x10
    b.ne finish
    mov w19, #2
    mov x0, x20
    bl stock_clock_on
    cbz w0, finish
    ldr x9, [x20, #0x18]
    /* Snapshot before recovery, while clocks are held. Mode != activity. */
    ldr w10, [x9, #0x40]
    str w10, [sp, #24]
    ldr w10, [x9, #0x258]
    str w10, [sp, #28]
    ldr w10, [x9, #0xe40]
    str w10, [sp, #32]
    ldr w10, [x9, #0xe18]
    str w10, [sp, #36]
    mov w19, #3
    ldr w10, [x9, #0x40]
    tbnz w10, #0, clocks_off
    tbnz w10, #12, clocks_off
    mov w19, #4
    ldr w10, [x9, #0x68]
    ubfx w10, w10, #8, #8
    cmp w10, #3
    b.ne clocks_off
    mov w19, #5
    /* BSP qcom-geni-se-common.h: DMA_TX_ACTIVE=BIT(0), RX=BIT(1). */
    ldr w10, [x9, #0xe40]
    tbnz w10, #0, clocks_off
    tbnz w10, #1, clocks_off
    mov w19, #6
    ldr w10, [x9, #0xe18]
    cbnz w10, gpi_snapshot
reload_checked:
    /* Recheck SE activity immediately before the stock loader. */
    ldr x9, [x20, #0x18]
    mov w19, #3
    ldr w10, [x9, #0x40]
    tbnz w10, #0, clocks_off
    tbnz w10, #12, clocks_off
    mov w19, #5
    ldr w10, [x9, #0xe40]
    tst w10, #3
    b.ne clocks_off
    mov x0, x20
    mov w1, #3
    bl reload
    mov w19, #7
    cbnz w0, clocks_off
    ldr x9, [x20, #0x18]
    mov w19, #8
    dsb sy
    ldr w10, [x9, #0x64]
    tbnz w10, #0, clocks_off
    ldr w10, [x9, #0x68]
    ubfx w10, w10, #8, #8
    cmp w10, #3
    b.ne clocks_off
    mov w19, #0
    b clocks_off
gpi_snapshot:
    /* Stock DT: GPI1 @ a00000, EE offset 10000, masks 1e | 1.
     * msm_gpi_mmio.h: context=20000+n*4000+k*80, scratch=+60.
     * SE5 requests dynamic assignment, NOT instance 5. Read AP 0..4
     * only. Never stop/reset a configured channel; reject any nonzero.
     */
    str wzr, [sp, #40]
gpi_next:
    ldr w2, [sp, #40]
    mov x9, #0xa1
    lsl x9, x9, #16
    add x9, x9, x2, lsl #14
    ldr w3, [x9]
    ldr w4, [x9, #0x60]
    ldr w5, [x9, #0x80]
    ldr w6, [x9, #0xe0]
    ldr w8, [sp, #44]
    orr w8, w8, w3
    orr w8, w8, w4
    orr w8, w8, w5
    orr w8, w8, w6
    str w8, [sp, #44]
    ldr w8, [sp, #40]
    add w8, w8, #1
    str w8, [sp, #40]
    cmp w8, #5
    b.lo gpi_next
    ldr w8, [sp, #44]
    cbnz w8, clocks_off
    /* Only the observed, documented low-four event bits are accepted.
     * Zero AP contexts are a bounded trial condition, not a cross-EE lock.
     */
    ldr w8, [sp, #36]
    cmp w8, #15
    b.ne clocks_off
    /* 9472: this path hung before returning. Zero contexts do not qualify
     * live reload. Keep the evidence but refuse enabled-GSI takeover. */
    b clocks_off
clocks_off:
    mov x0, x20
    bl stock_clock_off
    cbnz w0, finish
    mov w19, #9
finish:
    mov w8, w19
    ldp x19, x30, [sp], #64
    cbnz w8, fail
    b stock_success
fail:
    b stock_failure
.balign 4
reload:
    /* Exact original prologue/state setup 0x6e68..0x6e9c.
     * Omit only the same-protocol early return: that return cannot repair
     * a disabled FIFO. No global alteration of the stock loader.
     */
    sub sp, sp, #0x50
    stp x29, x30, [sp, #0x10]
    stp x24, x23, [sp, #0x20]
    stp x22, x21, [sp, #0x30]
    stp x20, x19, [sp, #0x40]
    add x29, sp, #0x10
    ldr x22, [x0, #0x18]
    ldr x8, [x0]
    add w9, w22, #0x68
    ldr x24, [x8, #0x18]
    add w8, w22, #0xe2c
    ldr w9, [x9]
    ldr w23, [x8]
    str xzr, [sp, #0x8]
    b stock_loader_body
