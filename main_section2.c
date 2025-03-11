// Week 4: Energy-efficient real time scheduler
// Important! DO NOT MODIFY this file. You will not submit this file.
// This file is provided for your implementation of the program procedure.
// For more details, please see the instructions in the class website.

#include <stdio.h>
#include <pthread.h>
#include <wiringPi.h>
#include <softPwm.h>

#include <stdlib.h>
#include <signal.h>

#include "workload.h"
#include "assignment1.h"
#include "assignment2.h"

#include "scheduler.h"
#include "governor.h"

#define thread_def(NAME) \
int bExit_##NAME; \
void* thread_##NAME(void* param) { \
	SharedVariable* pV = (SharedVariable*) param; \
	workload_##NAME(); \
	body_##NAME(pV); \
	bExit_##NAME = 1; \
	return NULL; }

thread_def(button)
thread_def(twocolor)
thread_def(motion)
thread_def(encoder)
thread_def(sound)
thread_def(rgbcolor)
thread_def(aled)
thread_def(buzzer)

void signal_handler(int signum) {
	finish_workload();
	release_buffer(fileno(stdout));
	printf("\nProgram exits by SIGINT.\n");
	exit(0);
}

int main(int argc, char* argv[]) {
	SharedVariable sv;

	int runningTimeInSec = 10;
	if (argc == 2) {
		runningTimeInSec = atoi(argv[1]);
	}

	printf("To run scheduler for %d seconds\n", runningTimeInSec);

	if (wiringPiSetup() == -1) {
		printf("Failed to setup wiringPi.");
		return 1; 
	}

	// Initialize for the interfaces provided
	signal(SIGINT, signal_handler);
	init_deferred_buffer(128*1024*1024); // 128MB
	init_userspace_governor();
	init_workload();

	// Initializers that you need to implement
	init_shared_variable(&sv);
	init_sensors(&sv);
	learn_workloads(&sv);

	// Init scheduler
	int aliveTasks[NUM_TASKS];
	init_scheduler(runningTimeInSec);
	set_by_max_freq(); // reset to the max freq

	printDBG("Start Scheduling with %d threads\n", NUM_TASKS);
	TaskSelection sel;
	long long idleTime;
	while (sv.bProgramExit != 1) {
		// 1. Prepare tasks
		// This function is implemented in scheduler.o, hidden to you
		idleTime = prepare_tasks(aliveTasks, &sv);
		if (idleTime < 0)
			break;

		// 2. Select a process: You need to implement.
		// Skeleton is given in assignment2.c
		sel = select_task(&sv, aliveTasks, idleTime); 
		if (sel.task < 0)
			break;
		
		// printf("selected task %d at frequency %d\n", sel.task, sel.freq);

		// 3. Run the selected task
		// This function is implemented in scheduler.o, hidden to you
		execute_task(sel);
	}

	finish_workload();
	release_buffer(fileno(stdout));
	printf("Scheduler program finished.\n");

    // Kevin's Code Changes
    printf("Low Freq Time: %f\n", sv.totalLowTime * 1e-6);
    printf("High Freq Time: %f\n", sv.totalHighTime * 1e-6);
    printf("Idle Time: %f\n", sv.totalIdleTime * 1e-6);
    printf("Energy consumed: %f\n", ((sv.totalHighTime * 2000.0) + (sv.totalLowTime * 1200.0) + (sv.totalIdleTime * 50.0)) * 1e-9);
    // end of code changes

	return 0;
}
