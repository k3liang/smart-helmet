all:
	gcc -o main_section1 main_section1.c sensor_interaction.c read_serial.c -lwiringPi -lpthread $(shell python3-config --embed --cflags --ldflags)

clean:
	rm -rf main_section1
