#include "eeprom.h"
#include "pinout.h"

void EEPROM::setDataDirection(int direction)
{
	static int data_direction = -1;

    if(data_direction == direction) return; // The data direction is correct and does not need to be set

    data_direction = direction;

    for(int pin = EEPROM_D0; pin <= EEPROM_D7; pin++)
        pinMode(pin, direction);
}

void EEPROM::setAddress(uint16_t address)
{
    shiftOut(SERIAL_DATA, SHIFT_CLK_HIGH, MSBFIRST, (address >> 8) & 0xFF);
	shiftOut(SERIAL_DATA, SHIFT_CLK_LOW,  MSBFIRST, address & 0xFF);
	digitalWrite(LATCH_CLK, HIGH);
	digitalWrite(LATCH_CLK, LOW);
}

byte EEPROM::readByte(uint16_t address)
{
    EEPROM::setAddress(address);
  	EEPROM::setDataDirection(INPUT);
    digitalWrite(EEPROM_OE, LOW);
    delayMicroseconds(1);
	byte data;
	for(int pin = EEPROM_D7; pin >= EEPROM_D0; pin--)
	{
		data = (data << 1) + digitalRead(pin);
	}
    digitalWrite(EEPROM_OE, HIGH);
	return data;
}

void EEPROM::writeByte(uint16_t address, uint8_t data)
{
    EEPROM::setAddress(address);
	for(int pin = EEPROM_D0; pin <= EEPROM_D7; pin++)
	{
		digitalWrite(pin, data & 0b1);
		data >>= 1;
	}
	digitalWrite(EEPROM_WE, LOW);
	delayMicroseconds(1);
	digitalWrite(EEPROM_WE, HIGH);
}

void EEPROM::writePage(uint16_t address, uint8_t* data)
{
    // A bitwise and with first X bits could be used to ensure 64 byte boundary of address
    EEPROM::setDataDirection(OUTPUT);
    for(uint32_t offset = 0; offset < 64; offset++)
		EEPROM::writeByte(address + offset, data[offset]);
}

void EEPROM::writeBytes(uint16_t address, uint8_t* data, uint16_t size)
{
    EEPROM::setDataDirection(OUTPUT);
	for(uint32_t offset = 0; offset < size; offset++)
	{
		EEPROM::writeByte(address + offset, data[offset]);
	    delay(5);
	}
}