#pragma once

#include <stdint.h>
#include <termios.h>

#define PORT_TIMEOUT -1
#define PORT_OK       0
#define PORT_ACK     'A'
#define PORT_NAK     'N'
#define PORT_RDY     'R'
#define PORT_ERR     'E'

struct SerialCommConfig
{
    uint8_t lsb_first; // 0: MSB first | 1: LSB first
    size_t status_await_timeout;
    int control_flags;
    int baud_rate;
};

struct SerialComm
{
    int port_fd;
    int status;
    struct termios options;
    struct SerialCommConfig config;
    uint8_t* send_buffer;
    uint8_t* receive_buffer;
    size_t send_buffer_size;
    size_t receive_buffer_size;
};

// Serial Communication Port Initialisation, Deinitialisation and Configuration
int SerialCommOpenPort(struct SerialComm* serial_port, const char* port_path, size_t buffer_size);
void SerialCommClosePort(struct SerialComm* serial_port);
int SerialCommApplyOptions(struct SerialComm* serial_port);
void SerialCommSetTimeout(struct SerialComm* serial_port, size_t s);
void SerialCommSetLSBFirst(struct SerialComm* serial_port, uint8_t lsb_first);
void SerialCommSetBaudrate(struct SerialComm* serial_port, int baud_rate);

int SerialCommDataAvailable(struct SerialComm* serial_port);

void SerialCommSendByte(struct SerialComm* serial_port, uint8_t data);
void SerialCommSendU16(struct SerialComm* serial_port, uint16_t data);
void SerialCommSendU32(struct SerialComm* serial_port, uint32_t data);

int SerialCommReadPortAll(struct SerialComm* serial_port);
int SerialCommReadBytes(struct SerialComm* serial_port, size_t bytes_to_read);
uint16_t SerialCommReadU16(struct SerialComm* serial_port);
uint32_t SerialCommReadU32(struct SerialComm* serial_port);

void SerialCommAwaitData(struct SerialComm* serial_port);
int SerialCommAwaitBytes(struct SerialComm* serial_port, int no_bytes);
int SerialCommAwaitStatus(struct SerialComm* serial_port);