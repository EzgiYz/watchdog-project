#ifndef SIGNAL_MANAGER_H
#define SIGNAL_MANAGER_H

#include <signal.h>

/* Sinyal olaylarini tutan bayraklar */
extern volatile sig_atomic_t shutdown_flag;
extern volatile sig_atomic_t child_flag;
extern volatile sig_atomic_t alarm_flag;
extern volatile sig_atomic_t status_flag;

/* Sinyal handler'larini kurar */
int setup_signal_handlers(void);

/* Periyodik kontrol zamanlayicisini baslatir */
int start_periodic_timer(void);

/* Kontrol zamanlayicisini durdurur */
int stop_periodic_timer(void);

#endif
