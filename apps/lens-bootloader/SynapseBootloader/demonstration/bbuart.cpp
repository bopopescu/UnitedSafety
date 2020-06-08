// C library headers

#include <stdio.h>
#include <string.h>

// Linux headers
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>

#include "bbuart.h"

int snapUartConfig(void) {

    int serial_port = open("/dev/ttySP2", O_RDWR);
    //int serial_port = open("/dev/ttyUSB0", O_RDWR);

    // check for UART Error
    if (serial_port < 0) {
        printf("Error %i from open: %s\n", errno, strerror(errno));
    }
    
    // Create a new termios struct
    struct termios tty;
    
    memset(&tty, 0, sizeof tty);
    
    // read the existing settings and handle any error
    if (tcgetattr(serial_port, &tty) != 0) {
        printf("Error %i from tcgetatter: %s\n", errno, strerror(errno));
    }
    
    // clear the parity bit
    tty.c_cflag &= ~PARENB;
    
    // set to only one stop bit by clearning the stop bit.
    tty.c_cflag &= ~CSTOPB;
    
    // set to 8 bits per byte
    tty.c_cflag |= CS8;
    
    // remove flow control
    tty.c_cflag &= ~CRTSCTS;
    
    // set the UART for reading and local (no modem detect lines)
    tty.c_cflag |= CREAD | CLOCAL;
    
    // disable canonical mode allows for byte by byte processing
    tty.c_lflag &= ~ICANON;
    
    // turn off echo
    tty.c_lflag &= ~ECHO;
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ECHONL;
    
    // remove signal characters
    tty.c_lflag &= ~ISIG;
    
    // remove software flow control
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    
    // setup to not preprocess data, we want raw data
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    
    // remove processing on output
    tty.c_oflag &= ~OPOST;
    tty.c_oflag &= ~ONLCR;
    // tty.c_oflag &= ~ONOEOT;  // Not in linux
    
    
    // set up our read timeout
    tty.c_cc[VTIME] = 1;  // wait one second for some data
    //tty.c_cc[VTIME] = 255;  // wait 25.5 seconds for some data
    tty.c_cc[VMIN] = 0; // no minimum number of characters
    
    // setup our baud rate
    cfsetispeed(&tty, B38400);
    cfsetospeed(&tty, B38400);
    
    // save the settings
    if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
        printf("Error %i from tcsetatter: %s\n", errno, strerror(errno));
    }
    return serial_port;
}

void snapUartClose(int serial_port) {
    close(serial_port);
}

void snapUartWrite(int serial_port, unsigned char * buf, size_t length)
{
    write(serial_port, buf, length);
}

int snapUartRead(int serial_port, char * buf, size_t buf_length)
{
    int n = read(serial_port, buf, buf_length);
    return n;
}
