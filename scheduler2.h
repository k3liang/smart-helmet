#ifndef _SCHEDULER_BODY_
#define _SCHEDULER_BODY_

#include <stdint.h>

unsigned long long get_current_time_us();

void learn_exectimes(SharedVariable* sv);
void run_task(SharedVariable* sv);

void tuneOn(SharedVariable *sv);
void tuneOff(SharedVariable *sv);

#endif