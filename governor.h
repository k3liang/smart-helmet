/**
 * IMPORTANT, this file and governor.c is copied from Assignment 2
 * in order to help us change CPU frequency.
 */

#ifndef _GOVERNOR_H_
#define _GOVERNOR_H_

// Assume single core use
void init_userspace_governor();
int write_driver(const char*, const char*);
void set_by_max_freq();
void set_by_min_freq();
int get_cur_freq();

void set_userspace_governor();
void set_ondemand_governor();

#endif