#include <stdlib.h>
#include <time.h>
#include "SerialComm.h"

#ifdef _WIN32

int SerialCommOpenPort(struct SerialComm* p, const char* p_path, size_t buffer_size)
{
    // Make a check to ensure that the buffer size provided is > 0

    /* Open the serial port file */
    p->hport = CreateFile(p_path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(p->hport == INVALID_HANDLE_VALUE){ return 0; }

    Sleep(2000); // Allow the arduino time to reset

    /* Allocate memory for the buffers */
    p->send_buffer = malloc(buffer_size); // Maybe make the send buffer a fixed size Max data that we would ever send would be 8 bytes for U64
    p->receive_buffer = malloc(buffer_size);
    p->send_buffer_size = buffer_size;
    p->receive_buffer_size = buffer_size;

    /* Initialise and set config to default values */
    SecureZeroMemory(&p->options, sizeof(DCB));
    p->options.DCBlength = sizeof(DCB);

    if(!GetCommState(p->hport, &p->options))
        return 0;

    p->options.BaudRate = CBR_9600;
    p->options.ByteSize = 8;
    p->options.Parity   = NOPARITY;
    p->options.StopBits = ONESTOPBIT;

    return 1;
}

void SerialCommSetBaudrate(struct SerialComm* p, int baud_rate)
{
    p->options.BaudRate = baud_rate;
}

int SerialCommApplyOptions(struct SerialComm* p)
{
    return SetCommState(p->hport, &p->options);
}

int SerialCommDataAvailable(struct SerialComm* p)
{
    COMSTAT stat;
    ClearCommError(p->hport, NULL, &stat);
    return stat.cbInQue;
}

void SerialCommSendBytesExt(struct SerialComm* port, void* src, size_t count)
{
    long unsigned int bytes_written;
    WriteFile(port->hport, src, count, &bytes_written, NULL);
}

int SerialCommReadBytesExt(struct SerialComm* port, void* dest, size_t count)
{
    // Buffer size check!!!
    SerialCommAwaitBytes(port, count);
    if(port->status == PORT_TIMEOUT) return 0;
    long unsigned int bytes_read;
    ReadFile(port->hport, dest, count, &bytes_read, NULL);
    return bytes_read;
}

#else

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

int SerialCommOpenPort(struct SerialComm* p, const char* p_path, size_t buffer_size)
{
    // Make a check to ensure that the buffer size provided is > 0

    /* Open the serial port file */
    p->port_fd = open(p_path, O_RDWR | O_NDELAY | O_NOCTTY);
    if(p->port_fd < 0){ return 0; }

    /* Allocate memory for the buffers */
    p->send_buffer = malloc(buffer_size); // Maybe make the send buffer a fixed size Max data that we would ever send would be 8 bytes for U64
    p->receive_buffer = malloc(buffer_size);
    p->send_buffer_size = buffer_size;
    p->receive_buffer_size = buffer_size;

    /* Set config to default values */
    p->options.c_cflag = B9600 | CS8 | CLOCAL | CREAD;
    p->options.c_iflag = IGNPAR;
    p->options.c_oflag = 0;
    p->options.c_lflag = 0;

    return 1;
}

void SerialCommSetBaudrate(struct SerialComm* p, int baud_rate)
{
    cfsetospeed(&p->options, baud_rate);
    cfsetispeed(&p->options, baud_rate);
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

void SerialCommSendBytesExt(struct SerialComm* port, void* src, size_t count)
{
    write(port->port_fd, src, count);
}

int SerialCommReadBytesExt(struct SerialComm* serial_port, void* dest, size_t bytes_to_read)
{
    // Buffer size check!!!
    SerialCommAwaitBytes(serial_port, bytes_to_read);
    if(serial_port->status == PORT_TIMEOUT) return 0;
    return read(serial_port->port_fd, dest, bytes_to_read);
}

#endif

void SerialCommClosePort(struct SerialComm* p)
{
    /* Close the serial port file */
#ifdef _WIN32
    CloseHandle(p->hport);
#else
    close(p->port_fd);
#endif

    /* Free the buffers' memory */
    free(p->send_buffer);
    free(p->receive_buffer);
}

void SerialCommSendBytes(struct SerialComm* port, size_t count)
{
    SerialCommSendBytesExt(port, port->send_buffer, count);
}

void SerialCommSendByte(struct SerialComm* port, uint8_t data)
{
    SerialCommSendBytesExt(port, &data, 1);
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
    SerialCommSendBytes(port, 2);
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
    SerialCommSendBytes(port, 4);
}

int SerialCommReadPortAll(struct SerialComm* port)
{
    int bytes_present = SerialCommDataAvailable(port);
    if(bytes_present < 1){ return 0; }
    return SerialCommReadBytes(port, bytes_present);
}

int SerialCommReadBytes(struct SerialComm* port, size_t count)
{
    // Buffer size check!!!
    SerialCommAwaitBytes(port, count);
    if(port->status == PORT_TIMEOUT) return 0;
    return SerialCommReadBytesExt(port, port->receive_buffer, count);
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

int SerialCommAwaitStatus(struct SerialComm* port)
{
    // Setup time variables
    time_t current_time = time(NULL);
    time_t timeout_time = current_time + port->config.status_await_timeout;

    // Variable to track if status was sent
    port->status = PORT_TIMEOUT;

    // Await data until timeout
    while(current_time < timeout_time)
    {
        if(SerialCommDataAvailable(port)){ port->status = PORT_OK; break; }
        current_time = time(NULL);
    }

    if(port->status == PORT_TIMEOUT) return 1;

    // If we did not time out then read the data on the port into the status field
    // We can make the assumption that there is at least 1 byte of data on the port
    // Technically there should be a check here to ensure the data was read correctly but we will ignore that for now
    SerialCommReadBytesExt(port, &port->status, 1);
    return 0;
}