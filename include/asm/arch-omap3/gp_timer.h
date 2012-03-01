#ifndef __IGEP_GPTIMER_H__
#define __IGEP_GPTIMER_H__

int timer_init (void);
void reset_timer (void);
ulong get_timer(ulong base);
void set_timer(ulong t);
void __udelay(unsigned long usec);

#endif
