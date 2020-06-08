#ifndef __SNAPUART_H__
#define __SNAPUART_H__

int snapUartConfig(void); 

void snapUartClose(int serial_port);

void snapUartWrite(int serial_port, unsigned char * buf, size_t length);

int snapUartRead(int serial_port, char * buf, size_t buf_length);

#endif
