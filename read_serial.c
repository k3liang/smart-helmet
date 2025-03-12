#include "read_serial.h"

/**
 * Helper functions that add to the wiringPi library serial functions
 */

char* readSerialData(const int fd, char* buffer) {
    int index = 0;
    int availableData;
    int ch;

    // Clear the buffer before reading the new data
    memset(buffer, 0, MAX_LINE_LENGTH);

    availableData = serialDataAvail(fd);

    // Read available data from the serial port
    while (index < availableData && index < MAX_LINE_LENGTH - 1) {
        ch = serialGetchar(fd); 

        if (ch == -1) {
            break;
        }

        buffer[index++] = (char)ch; 
    }

    // Null-terminate the buffer to make it a valid C string
    buffer[index] = '\0';

    return buffer;  
}

char* readSerialLine(const int fd, char* line) {
    int index = 0;
    int ch;

    // Clear the buffer before reading the new line
    memset(line, 0, MAX_LINE_LENGTH);

    while (index < MAX_LINE_LENGTH - 1) {
        ch = serialGetchar(fd); 

        if (ch == -1) {
            continue;
        }

        if (ch == '\n') {
            // End of line, terminate the string
            if (index > 0 && line[index-1] == 13) {
                line[index-1] = '\0';
                break;
            }
            line[index] = '\0';
            break;
        }

        line[index++] = (char)ch;
    }
    return line; 
}

char* readSerialLine2(const int fd, char* line, unsigned int timeout) {
    int index = 0;
    int ch;
    unsigned int startTime = millis();

    // Clear the buffer before reading the new line
    memset(line, 0, MAX_LINE_LENGTH);

    // run until we reach the timeout
    while (millis() - startTime <= timeout) {
        ch = serialGetchar(fd); 

        if (ch == -1) {
            continue;
        }

        if (ch == '\n') {
            // End of line, terminate the string
            if (index > 0 && line[index-1] == 13) {
                line[index-1] = '\0';
                return line;
            }
            line[index] = '\0';
            return line;
        }

        line[index++] = (char)ch;
    }
    memset(line, 0, MAX_LINE_LENGTH);
    strcpy(line, "-1");
    return NULL;  // Return error
}