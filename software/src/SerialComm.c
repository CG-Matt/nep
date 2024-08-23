#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include "SerialComm.h"

int SerialCommOpenPort(struct SerialComm* p, const char* p_path, size_t buffer_size)
{
    // Make a check to ensure that the buffer size provided is > 0

    /* Open the serial port file */
    p->port_fd = open(p_path, O_RDWR | O_NDELAY | O_NOCTTY);
    if(p->port_fd < 0){ return p->port_fd; }

    /* Allocate memory for the buffers */
    p->send_buffer = malloc(buffer_size); // Maybe make the send buffer a fixed size Max data that we would ever send would be 8 bytes for U64
    p->receive_buffer = malloc(buffer_size);
    p->send_buffer_size = buffer_size;
    p->receive_buffer_size = buffer_size;

    /* Set config to default values */
    p->config.control_flags = CS8 | CLOCAL | CREAD;
    p->options.c_cflag = B9600 | p->config.control_flags;
    p->options.c_iflag = IGNPAR;
    p->options.c_oflag = 0;
    p->options.c_lflag = 0;

    return 0;
}

void SerialCommClosePort(struct SerialComm* p)
{
    /* Close the serial port file */
    close(p->port_fd);

    /* Free the buffers' memory */
    free(p->send_buffer);
    free(p->receive_buffer);
}

void SerialCommSetBaudrate(struct SerialComm* p, int baud_rate)
{
    p->options.c_cflag = baud_rate | p->config.control_flags;
}

int SerialCommApplyOptions(struct SerialComm* port)
{
    tcflush(port->port_fd, TCIFLUSH);
    return tcsetattr(port->port_fd, TCSANOW, &port->options) == 0 ? 1 : 0;
}

int SerialCommDataAvailable(struct SerialComm* serial_port)
{
    int bytes_present;
    ioctl(serial_port->port_fd, FIONREAD, &bytes_present);
    return bytes_present;
}

void SerialCommSendByte(struct SerialComm* port, uint8_t data)
{
    write(port->port_fd, &data, 1);
}

void SerialCommSendU16(struct SerialComm* port, uint16_t data)
{
    // Check send buffer size
    if(port->send_buffer_size < 2)
    {
        port->status = PORT_ERR;
        return;
    }

    // Place the data into the send_buffer
    if(port->config.lsb_first)
    {
        for(size_t i = 0; i < 2; i++)
            port->send_buffer[i] = (data >> (i * 8)) & 0xFF;
    }
    else
    {
        for(size_t i = 0; i < 2; i++)
            port->send_buffer[i] = (data >> ((1 - i) * 8)) & 0xFF;
    }

    // Send data over the serial port
    write(port->port_fd, port->send_buffer, 2);
}

void SerialCommSendU32(struct SerialComm* port, uint32_t data)
{
    // Check send buffer size
    if(port->send_buffer_size < 4)
    {
        port->status = PORT_ERR;
        return;
    }

    // Place the data into the send_buffer
    if(port->config.lsb_first)
    {
        for(size_t i = 0; i < 4; i++)
            port->send_buffer[i] = (data >> (i * 8)) & 0xFF;
    }
    else
    {
        for(size_t i = 0; i < 4; i++)
            port->send_buffer[i] = (data >> ((3 - i) * 8)) & 0xFF;
    }

    // Send data over the serial port
    write(port->port_fd, port->send_buffer, 4);
}

int SerialCommReadPortAll(struct SerialComm* serial_port)
{
    int bytes_present = SerialCommDataAvailable(serial_port);
    if(bytes_present < 1){ return 0; }
    int bytes_read = read(serial_port->port_fd, serial_port->receive_buffer, bytes_present);
    return bytes_read;
}

int SerialCommReadBytes(struct SerialComm* serial_port, size_t bytes_to_read)
{
    SerialCommAwaitBytes(serial_port, bytes_to_read);
    if(serial_port->status == PORT_TIMEOUT) return 0;
    return read(serial_port->port_fd, serial_port->receive_buffer, bytes_to_read);
}

uint16_t SerialCommReadU16(struct SerialComm* port)
{
    SerialCommReadBytes(port, 2);

    if(port->status == PORT_TIMEOUT) return 0;
    
    uint32_t ret;

    if(port->config.lsb_first)
    {
        for(size_t i = 0; i < 2; i++)
        {
            ret <<= 8;
            ret |= port->receive_buffer[1 - i];
        }
    }
    else
    {
        for(size_t i = 0; i < 2; i++)
        {
            ret <<= 8;
            ret |= port->receive_buffer[i];
        }
    }

    return ret;
}

/*
    Awaits and reads in a u32 from the serial port
    Port status will be set to timeout if we did not receive the data in time
*/
uint32_t SerialCommReadU32(struct SerialComm* port)
{
    SerialCommReadBytes(port, 4);

    if(port->status == PORT_TIMEOUT) return 0;

    uint32_t ret;

    if(port->config.lsb_first)
    {
        for(size_t i = 0; i < 4; i++)
        {
            ret <<= 8;
            ret |= port->receive_buffer[3 - i];
        }
    }
    else
    {
        for(size_t i = 0; i < 4; i++)
        {
            ret <<= 8;
            ret |= port->receive_buffer[i];
        }
    }

    return ret;
}

void SerialCommSetTimeout(struct SerialComm* serial_port, size_t s)
{
    serial_port->config.status_await_timeout = s;
}

void SerialCommSetLSBFirst(struct SerialComm* port, uint8_t lsb_first)
{
    port->config.lsb_first = lsb_first;
}

void SerialCommAwaitData(struct SerialComm* p)
{
    // Setup time variables
    time_t current_time = time(NULL);
    time_t timeout_time = current_time + p->config.status_await_timeout;

    // Variable to track if data has been sent
    int timeout = 1;

    // Await data until timeout
    while(current_time < timeout_time)
    {
        if(SerialCommDataAvailable(p)){ timeout = 0; break; }
        current_time = time(NULL);
    }

    p->status = timeout ? PORT_TIMEOUT : PORT_OK;
    return;
}

int SerialCommAwaitBytes(struct SerialComm* p, int nbytes)
{
    // Setup time variables
    time_t current_time = time(NULL);
    time_t timeout_time = current_time + p->config.status_await_timeout;

    // Variable to track if status was sent
    int ok = 0;

    // Await data until timeout
    // Maybe make the timeout time reset every time we receive a byte
    while(current_time < timeout_time)
    {
        if(SerialCommDataAvailable(p) >= nbytes){ ok = 1; break; }
        current_time = time(NULL);
    }

    // If we timed out, return error
    if(!ok)
    {
        p->status = PORT_TIMEOUT;
        return -1;
    }

    p->status = PORT_OK;
    return 0;
}

int SerialCommAwaitStatus(struct SerialComm* serial_port)
{
    // Setup time variables
    time_t current_time = time(NULL);
    time_t timeout_time = current_time + serial_port->config.status_await_timeout;

    // Variable to track if status was sent
    int ok = 0;

    // Await data until timeout
    while(current_time < timeout_time)
    {
        if(SerialCommDataAvailable(serial_port)){ ok = 1; break; }
        current_time = time(NULL);
    }

    // If we timed out, return
    if(!ok)
    {
        serial_port->status = PORT_TIMEOUT;
        return -1;
    }

    // If we did not time out then read the data on the port into the status field
    // We can make the assumption that there is at least 1 byte of data on the port
    // Technically there should be a check here to ensure the data was read correctly but we will ignore that for now
    serial_port->status = PORT_OK;
    read(serial_port->port_fd, &serial_port->status, 1);
    return 0;
}