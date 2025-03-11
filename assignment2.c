#include "assignment1.h"
#include "assignment2.h"
#include "workload.h"
#include "scheduler.h"
#include "governor.h"

#include <stdio.h>
#include <limits.h>

static taskInfo tasks[NUM_TASKS];
static double slackUtil;


// Note: Deadline of each workload is defined in the "workloadDeadlines" variable.
// i.e., You can access the dealine of the BUTTON thread using workloadDeadlines[BUTTON]

// Assignment: You need to implement the following two functions.

// learn_workloads(SharedVariable* v):
// This function is called at the start part of the program before actual scheduling
// - Parameters

// sv: The variable which is shared for every function over all threads
void learn_workloads(SharedVariable* v) {
	// TODO: Fill the body
	// This function is executed before the scheduling simulation.
	// You need to calculate the execution time of each thread here.
	//
	// Thread functions: 
	// thread_button, thread_touch, thread_encoder, thread_tilt,
	// thread_twocolor, thread_rgbcolor, thread_aled, thread_buzzer

	// Tip 1. You can call each workload function here like:
	// thread_button();

	// Tip 2. You can get the current time here like:
	// long long curTime = get_current_time_us();

    void* (*thread_funcs[NUM_TASKS]) (void*) = {thread_button, thread_twocolor, thread_motion, thread_encoder, thread_sound, thread_rgbcolor, thread_aled, thread_buzzer};

    int i;
    for (i = 0; i < NUM_TASKS; ++i) {
        tasks[i].aliveTask_prev = 0;
        tasks[i].lowRunning = 0;
        tasks[i].nextDeadline = workloadDeadlines[i];
    }

    v->totalIdleTime = 0;
    v->totalLowTime = 0;
    v->totalHighTime = 0;

    double totalUtil = 0.0;

    set_by_min_freq();
    for (i = 0; i < NUM_TASKS; ++i) {
        void* (*thread_func)(void*) = thread_funcs[i];
        long long startTime = get_current_time_us();
        thread_func(v);
        long long endTime = get_current_time_us();
        tasks[i].lowFreqTime = endTime - startTime+350;
        tasks[i].lowUtil = (double)(endTime-startTime+350) / (double)workloadDeadlines[i];
    }

    set_by_max_freq();
    for (i = 0; i < NUM_TASKS; ++i) {
        void* (*thread_func)(void*) = thread_funcs[i];
        long long startTime = get_current_time_us();
        thread_func(v);
        long long endTime = get_current_time_us();
        tasks[i].highFreqTime = endTime - startTime+250;
        tasks[i].highUtil = (double)(endTime-startTime+250) / (double)workloadDeadlines[i];
        tasks[i].diffUtil = tasks[i].lowUtil - tasks[i].highUtil;
        totalUtil += tasks[i].highUtil;
    }

    slackUtil = 0.75 - totalUtil;
    slackUtil /= 1.7;
}


// select_task(SharedVariable* sv, const int* aliveTasks):
// This function is called while runnning the actual scheduler
// - Parameters
// sv: The variable which is shared for every function over all threads
// aliveTasks: an array where each element indicates whether the corresponed task is alive(1) or not(0).
// idleTime: a time duration in microsecond. You can know how much time was waiting without any workload
//           (i.e., it's larger than 0 only when all threads are finished and not reach the next preiod.)
// - Return value
// TaskSelection structure which indicates the scheduled task and the CPU frequency
TaskSelection select_task(SharedVariable* sv, const int* aliveTasks, long long idleTime) {
	// TODO: Fill the body
	// This function is executed inside of the scheduling simulation.
    // You need to implement a EDF scheduler.

    // Goal: Select a task that has the earliest deadline among the ready tasks.

	// Tip 1. You may get the current time elapsed in the scheduler here like:
	// long long curTime = get_scheduler_elapsed_time_us();

	// Tip 2. The readiness and completeness of tasks are indicated by
	// the flip of aliveTasks.
	// For example, when aliveTasks[i] switches from 1 (alive) to 0 
	// (not alive), it indicates that task i is finished for the current
	// round.
	// When aliveTasks[i] switches from 0 to 1, it means task i is ready
	// to schedule for its next period.

	// DO NOT make any interruptable / IO tasks in this function.
	// You can use printDBG instead of printf.

    
	static int prev_selection = -1;
    static int freq_selection = 1;
    static long long prevTime = 0;

    long long curTime = get_scheduler_elapsed_time_us();
    TaskSelection sel;

    sv->totalIdleTime += idleTime;
    if (freq_selection == 1) {
        sv->totalHighTime += curTime - prevTime - idleTime;
    } else {
        sv->totalLowTime += curTime - prevTime - idleTime;
    }
    prevTime = curTime;

    long long earliestDL = LLONG_MAX;
    int i;

    for (i = 0; i < NUM_TASKS; ++i) {
        if (tasks[i].aliveTask_prev==1 && aliveTasks[i]==0) {
            tasks[i].nextDeadline += workloadDeadlines[i];
        }
        tasks[i].aliveTask_prev = aliveTasks[i];
    }

	for (i = 0; i < NUM_TASKS; ++i) {
		if (aliveTasks[i] == 1) {
            if (tasks[i].nextDeadline < earliestDL) {
                earliestDL = tasks[i].nextDeadline;
                prev_selection = i;
            }
		}
	}

    if (tasks[prev_selection].lowRunning == 1) {
        freq_selection = 0;
    } else if (tasks[prev_selection].diffUtil < slackUtil) {
        tasks[prev_selection].lowRunning = 1;
        slackUtil -= tasks[prev_selection].diffUtil;
        freq_selection = 0;
    } else {
        freq_selection = 1;
    }

	// The return value can be specified like this:
	
	sel.task = prev_selection; // The thread ID which will be scheduled. i.e., 0(BUTTON) ~ 7(BUZZER)
	sel.freq = freq_selection; // Request the maximum frequency (if you want the minimum frequency, use 0 instead.)

    return sel;
}
