#include "timer.h"
#include "console.h"

#define PIT_BASE_FREQUENCY 1193182
#define PIT_CHANNEL_0      0x40
#define PIT_COMMAND_PORT   0x43

static volatile uint64_t timer_ticks = 0;
static uint32_t target_freq_hz = 1000;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void timer_init(uint32_t frequency_hz) {
    target_freq_hz = frequency_hz;
    uint16_t divisor = (uint16_t)(PIT_BASE_FREQUENCY / frequency_hz);

    // Command byte: Channel 0, Access mode low/high byte, Mode 3 (square wave)
    outb(PIT_COMMAND_PORT, 0x36);

    // Send divisor low byte, then high byte
    outb(PIT_CHANNEL_0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL_0, (uint8_t)((divisor >> 8) & 0xFF));

    klogf("pit0: initialized at %d Hz (divisor: %d)\n", frequency_hz, divisor);
}

// Call this from your IRQ 0 interrupt handler (Vector 32)
void timer_handler(void) {
    timer_ticks++;
}

uint64_t timer_get_ticks(void) {
    return timer_ticks;
}

uint64_t timer_get_uptime_ms(void) {
    return (timer_ticks * 1000) / target_freq_hz;
}

void sleep_ms(uint32_t ms) {
    uint64_t start_ms = timer_get_uptime_ms();
    while (timer_get_uptime_ms() - start_ms < ms) {
        // Halt CPU until next interrupt to avoid frying CPU cycles
        __asm__ volatile("hlt");
    }
}