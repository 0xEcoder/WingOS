.extern idt_handle_exception

# Define global visibility for all 32 ISRs + the Timer
.global isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7
.global isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15
.global isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
.global isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
.global irq0_timer

# Macro to save all general-purpose registers (15 total, 120 bytes)
.macro PUSHAD
    pushq %rax; pushq %rcx; pushq %rdx; pushq %rbx
    pushq %rbp; pushq %rsi; pushq %rdi; pushq %r8
    pushq %r9;  pushq %r10; pushq %r11; pushq %r12
    pushq %r13; pushq %r14; pushq %r15
.endm

# Macro to restore all general-purpose registers (Fixed the pushq %rcx bug!)
.macro POPAD
    popq %r15; popq %r14; popq %r13; popq %r12
    popq %r11; popq %r10; popq %r9;  popq %r8
    popq %rdi; popq %rsi; popq %rbp;  popq %rbx
    popq %rdx; popq %rcx; popq %rax
.endm

# ----------------------------------------------------
# Macros to auto-generate the 32 exception handlers
# ----------------------------------------------------
.macro ISR_NO_ERRCODE num
isr\num:
    pushq $0                 # Push a dummy error code
    pushq $\num              # Push the vector number
    jmp isr_common_stub
.endm

.macro ISR_ERRCODE num
isr\num:
                             # CPU already pushed error code!
    pushq $\num              # Push the vector number
    jmp isr_common_stub
.endm

# Generate all 32 standard exceptions
ISR_NO_ERRCODE 0   # Divide by Zero
ISR_NO_ERRCODE 1   # Debug
ISR_NO_ERRCODE 2   # NMI
ISR_NO_ERRCODE 3   # Breakpoint
ISR_NO_ERRCODE 4   # Overflow
ISR_NO_ERRCODE 5   # Bound Range Exceeded
ISR_NO_ERRCODE 6   # Invalid Opcode
ISR_NO_ERRCODE 7   # Device Not Available
ISR_ERRCODE    8   # Double Fault
ISR_NO_ERRCODE 9   # Coprocessor Segment Overrun
ISR_ERRCODE    10  # Invalid TSS
ISR_ERRCODE    11  # Segment Not Present
ISR_ERRCODE    12  # Stack-Segment Fault
ISR_ERRCODE    13  # General Protection Fault (#GP)
ISR_ERRCODE    14  # Page Fault (#PF)
ISR_NO_ERRCODE 15  # Reserved
ISR_NO_ERRCODE 16  # x87 Floating-Point Exception
ISR_ERRCODE    17  # Alignment Check
ISR_NO_ERRCODE 18  # Machine Check
ISR_NO_ERRCODE 19  # SIMD Floating-Point Exception
ISR_NO_ERRCODE 20  # Virtualization Exception
ISR_ERRCODE    21  # Control Protection Exception
ISR_NO_ERRCODE 22  # Reserved
ISR_NO_ERRCODE 23  # Reserved
ISR_NO_ERRCODE 24  # Reserved
ISR_NO_ERRCODE 25  # Reserved
ISR_NO_ERRCODE 26  # Reserved
ISR_NO_ERRCODE 27  # Reserved
ISR_NO_ERRCODE 28  # Hypervisor Injection Exception
ISR_ERRCODE    29  # VMM Communication Exception
ISR_ERRCODE    30  # Security Exception
ISR_NO_ERRCODE 31  # Reserved

# Hardware Timer Stub (Remapped IRQ 0 -> Vector 32)
irq0_timer:
    pushq $0                 # Dummy error code
    pushq $32                # Vector 32 (0x20)
    jmp isr_common_stub

# ----------------------------------------------------
# Common Routing Stub
# ----------------------------------------------------
isr_common_stub:
    PUSHAD                  # Push all 15 registers (decrements RSP by 120 bytes)

    # Calculate stack offsets to extract parameters for System V AMD64 ABI
    # %rdi = 1st arg (vector), %rsi = 2nd arg (error code), %rdx = 3rd arg (RIP)
    movq 120(%rsp), %rdi    # Vector number is 120 bytes up the stack
    movq 128(%rsp), %rsi    # Error code is 128 bytes up the stack
    movq 136(%rsp), %rdx    # Original RIP pushed by CPU is 136 bytes up the stack

    cld                     # Clear direction flag for GCC compliance
    call idt_handle_exception

    POPAD                   # Restore our 15 registers
    addq $16, %rsp          # Clean up the vector number and error code
    iretq                   # Return from interrupt back to normal code execution