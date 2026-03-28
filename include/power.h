#ifndef POWER_H
#define POWER_H

/* Reset the machine through standard PC firmware mechanisms. */
void power_restart(void);

/* Power off supported virtual machines and then halt. */
void power_shutdown(void);

#endif /* POWER_H */
