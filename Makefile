all:
	gcc -o main_section main_section.c sensor_interaction.c read_serial.c scheduler2.c governor.c -lwiringPi -lpthread $(shell python3-config --embed --cflags --ldflags)

clean:
	rm -rf main_section
