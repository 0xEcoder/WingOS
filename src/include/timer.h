#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void timer_init(uint32_t frequency_hz);
void timer_handler(void);
uint64_t timer_get_ticks(void);
uint64_t timer_get_uptime_ms(void);
void sleep_ms(uint32_t ms);

#endif