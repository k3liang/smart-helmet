// Important! DO NOT MODIFY this file.
// You will not submit this file.
// This file is provided for your understanding of the program procedure.

// Skeleton code of CSE237A, Sensor interaction
// For more details, please see the instructions on the class website.

#include <stdio.h>
#include <wiringPi.h>
#include <softPwm.h>
#include <signal.h>
#include "sensor_interaction.h"

volatile sig_atomic_t exit_flag = 0;
void signal_handler(int signum) {
	exit_flag = 1;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);

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
	if (init_python(&v) != 0) {
		printf("Failed to initialize Python.\n");
	}

	// Main program loop
	while (!exit_flag && v.bProgramExit != 1) {
        /*
		// Add a slight delay between iterations
		delay(10);
        */
       body_button(&v);
       body_encoder(&v);
       body_twocolor(&v);
       body_aled(&v);
       body_buzzer(&v);
       body_temphumid(&v);
       body_air(&v);
       body_accel(&v);
       body_camera(&v);
       body_lcd(&v);
       delay(2000);
	}
    printf("saving all manual calibrations");
    saveCalib(&v);

	printf("trying to clean up python...\n");
	clean_python(&v);

	printf("Program finished.\n");

	return 0;
}
