all:
	gcc -o main_section1 main_section1.c sensor_interaction.c read_serial.c -lwiringPi -lpthread

clean:
	rm -rf main_section1
