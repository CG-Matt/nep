#include <Arduino.h>
#include "pinout.h"
#include "eeprom.h"

#define FIRM_VER_MJR 0
#define FIRM_VER_MNR 1
#define FIRM_VER_PCH 0

#define PORT_TIMEOUT -1
#define PORT_ACK     'A'
#define PORT_NAK     'N'
#define PORT_RDY     'R'
#define PORT_ERR     'E'
#define PORT_SIG     'S'
#define PORT_READ    'R'
#define PORT_WRITE   'W'
#define PORT_DUMP    'B'
#define PORT_P_EN    'E'
#define PORT_P_DIS   'D'

#define P_WRITE_DELAY 7 // Delay between page writes in ms

void printContents()
{
	Serial.println("");
	for(uint16_t base = 0; base < 0x8000; base += 16)
	{
		byte data[16];
		for(uint16_t offset = 0; offset < 16; offset++)
		{
			data[offset] = EEPROM::readByte(base + offset);
		}
		char buffer[0x7F];
		sprintf(buffer, "%04X: %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX   %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX",
				base,
				data[0], data[1],  data[2],  data[3],  data[4],  data[5],  data[6],  data[7],
				data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]);

		Serial.println(buffer);
	}
}

/*
    Make a uint32_t from a buffer of 4 uint8_t
    Data must be LSB first
*/
inline uint32_t SerialShiftInU32()
{
    uint32_t ret;
    for(uint8_t i = 0; i < 4; i++)
    {
        while(!Serial.available()) continue;  // Loop until serial is available
        ((uint8_t*)(&ret))[i] = Serial.read() & 0xFF;
    }
    return ret;
}

/*
    Send out a uint32_t as 4 uint8_t
    Data will be LSB first
*/
inline void SerialShiftOutU32(uint32_t data)
{
    for(uint8_t i = 0; i < 4; i++)
        Serial.write(((uint8_t*)(&data))[i]);
}

byte page[256];

void handle_EEPROM_write()
{
    byte rx_buffer[8];
    uint32_t bytes_received = 0;
    uint32_t image_size = SerialShiftInU32();

    // Respond with acknowledge and and echo image size
    Serial.write(PORT_ACK);
    SerialShiftOutU32(image_size);

    while(!Serial.available()) continue;        // Await acknowledge from computer
    byte response = Serial.read();

    if(response != PORT_ACK)                    // Computer did not acknowledge return to idle
        return;

    uint32_t pages_received = 0;                // Number of pages that have been written
    bytes_received = 0;                         // Number of received bytes in a page

    // Change this to bitshift image_size and compare against that
    while((pages_received << 8) < image_size)   // Loop until all pages have been processed
    {
        Serial.write(PORT_READ);                // Tell the computer we are ready for the next page
    
        while(bytes_received < 256)             // Read in 256 bytes (1 page) from the serial port
        {
            while(!Serial.available()) continue;
            page[bytes_received] = Serial.read();
            bytes_received++;
        }
        Serial.write(PORT_ACK);                 // Acknowledge page received

        // Write the data to the EEPROM
        EEPROM::writePage(pages_received << 8, page);
        delay(P_WRITE_DELAY);
        EEPROM::writePage((pages_received << 8) + 0x40, page + 0x40);
        delay(P_WRITE_DELAY);
        EEPROM::writePage((pages_received << 8) + 0x80, page + 0x80);
        delay(P_WRITE_DELAY);
        EEPROM::writePage((pages_received << 8) + 0xC0, page + 0xC0);
        delay(P_WRITE_DELAY);

        // Check that the data was written to the EEPROM correctly
        for(size_t idx = 0; idx < 256; idx++)
        {
            byte byte_written = EEPROM::readByte((pages_received * 256) + idx);
            // This error routine need to be updated
            if(byte_written != page[idx])
            {
                Serial.write(PORT_ERR);
                Serial.write(idx);
                Serial.write(page[idx]);
                Serial.write(byte_written);
            }
        }

        // Increment page counter and reset bytes received
        pages_received++;
        bytes_received = 0;
    }
}

void handle_EEPROM_dump()
{
    uint32_t image_size = SerialShiftInU32();

    // Respond with acknowledge and echo image size
    Serial.write(PORT_ACK);
    SerialShiftOutU32(image_size);

    while(!Serial.available()) continue;    // Await acknowledge from computer
    byte response = Serial.read();

    if(response != PORT_ACK)                // Computer did not acknowledge return to idle
        return;

    uint32_t bytes_sent = 0;                // Keeps track of how many of the request bytes have been sent

    while(!Serial.available()) continue;    // Await read from the computer
    response = Serial.read();
    
    if(response != PORT_RDY)                // Unknown response
        return;

    while(bytes_sent < image_size)          // Loop until all pages have been processed
    {
        Serial.write(EEPROM::readByte(bytes_sent));
        bytes_sent++;
    }

    while(!Serial.available()) continue;    // Await acknowledge from computer
    response = Serial.read();

    if(response != PORT_ACK)                // Computer did not acknowledge return to idle
        return;

    Serial.write(PORT_ACK);                 // Acknowledge and return to idle
}

void setup()
{
    digitalWrite(LATCH_CLK, LOW);
	digitalWrite(EEPROM_WE, HIGH);
    digitalWrite(EEPROM_OE, HIGH);
    
	pinMode(SERIAL_DATA, OUTPUT);
	pinMode(SHIFT_CLK_LOW, OUTPUT);
	pinMode(SHIFT_CLK_HIGH, OUTPUT);
	pinMode(LATCH_CLK, OUTPUT);
	pinMode(EEPROM_WE, OUTPUT);
    pinMode(EEPROM_OE, OUTPUT);

	Serial.begin(115200);
}

void loop()
{
	while(!Serial.available()) continue;
	
	char command_type = Serial.read();

	if(command_type == PORT_SIG)    // Get Device Signature
	{
		Serial.write(PORT_ACK);	    // Acknowledge
		Serial.write(FIRM_VER_MJR);	// Firmware major version
		Serial.write(FIRM_VER_MNR);	// Firmware minor version
		Serial.write(FIRM_VER_PCH);	// Firmware patch version
		Serial.write(0x0A);         // Newline to finish transmission
        return;
	}

	if(command_type == PORT_READ) // Read data from EEPROM
	{
		printContents();
        Serial.write(0);  // Null byte to finish transmission
        return;
	}

    if(command_type == PORT_DUMP) // Binary dump of the EEPROM data
    {
        handle_EEPROM_dump();
        return;
    }

	if(command_type == PORT_WRITE) // Write data to the EEPROM
	{
        handle_EEPROM_write();
        return;
	}

    // Add some form of check to see if this was actually successful
    if(command_type == PORT_P_DIS) // Disable write protection
    {
        EEPROM::setDataDirection(OUTPUT);
        EEPROM::writeByte(0x5555, 0xAA);
        EEPROM::writeByte(0x2AAA, 0x55);
        EEPROM::writeByte(0x5555, 0x80);
        EEPROM::writeByte(0x5555, 0xAA);
        EEPROM::writeByte(0x2AAA, 0x55);
        EEPROM::writeByte(0x5555, 0x20);
        delay(P_WRITE_DELAY);
        return;
    }

    // Add some form of check to see if this was actually successful
    if(command_type == PORT_P_EN) // Enable write protection
    {
        EEPROM::setDataDirection(OUTPUT);
        EEPROM::writeByte(0x5555, 0xAA);
        EEPROM::writeByte(0x2AAA, 0x55);
        EEPROM::writeByte(0x5555, 0xA0);
        delay(P_WRITE_DELAY);
        return;
    }

    // Unknown command type
    Serial.write(PORT_NAK);
	return;
}