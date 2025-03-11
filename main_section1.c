// Important! DO NOT MODIFY this file.
// You will not submit this file.
// This file is provided for your understanding of the program procedure.

// Skeleton code of CSE237A, Sensor interaction
// For more details, please see the instructions on the class website.

#include <stdio.h>
#include <pthread.h>
#include <wiringPi.h>
#include <softPwm.h>
#include "sensor_interaction.h"

// Thread declaration macros
#define thread_decl(NAME) \
void* thread_##NAME(void* param) { \
	SharedVariable* pV = (SharedVariable*) param; \
	body_##NAME(pV); \
	return NULL; }

// Declare threads for each sensor/actuator function
thread_decl(button)
//thread_decl(motion)
//thread_decl(sound)
thread_decl(encoder)
thread_decl(twocolor)
//thread_decl(rgbcolor)
thread_decl(aled)
thread_decl(buzzer)

thread_decl(temphumid)
thread_decl(air)
thread_decl(accel)
thread_decl(lcd)

// Thread creation and joining macros
#define thread_create(NAME) pthread_create(&t_##NAME, NULL, thread_##NAME, &v);
#define thread_join(NAME) pthread_join(t_##NAME, NULL);

int main(int argc, char* argv[]) {
	// Initialize shared variable
	SharedVariable v;

	// Initialize WiringPi library
	if (wiringPiSetup() == -1) {
		printf("Failed to setup wiringPi.\n");
		return 1; 
	}

	// Initialize shared variable and sensors
    if (init_boundaries(&v) != 0) {
        printf("Error in reading from %s\n", SENSORFILE);
        return 1;
    }
	init_shared_variable(&v);
	init_sensors(&v);

	// Thread identifiers
	pthread_t t_button,
			  //t_motion,
			  //t_sound,
			  t_encoder,
			  t_twocolor,
			  //t_rgbcolor,
			  t_aled,
			  t_buzzer,
              t_temphumid,
              t_air,
              t_accel,
              t_lcd;

	// Main program loop
	while (v.bProgramExit != 1) {
        /*
		// Create sensing threads
		thread_create(button);
		//thread_create(motion);
		//thread_create(sound);
		thread_create(encoder);
		thread_create(twocolor);
		//thread_create(rgbcolor);
		thread_create(aled);
		thread_create(buzzer);

        thread_create(temphumid);
        thread_create(air);
        thread_create(accel);

		// Wait for all threads to finish
		thread_join(button);
		//thread_join(motion);
		//thread_join(sound);
		thread_join(encoder);
		thread_join(twocolor);
		//thread_join(rgbcolor);
		thread_join(aled);
		thread_join(buzzer);

        thread_join(temphumid);
        thread_join(air);
        thread_join(accel);

		// Add a slight delay between iterations
		delay(10);
        */
       body_temphumid(&v);
       body_air(&v);
       body_accel(&v);
       body_lcd(&v);
       delay(2000);
	}

	printf("Program finished.\n");

	return 0;
}
