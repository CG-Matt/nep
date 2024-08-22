#pragma once

#include <Arduino.h>

namespace EEPROM
{
    static const uint8_t pageSize = 0x40;

    /*
        Sets the direction of the data pins
        @param direction The direction to set the pins to from the perspective of the Arduino
    */
    void setDataDirection(int direction);

    void setAddress(uint16_t address);

    byte readByte(uint16_t address);

    /*
        REQUIRED: Data direction must be set prior to using
    */
    void writeByte(uint16_t address, uint8_t data);

    /*
        Program an EEPROM page (64 bytes) with provided data
        NOTE: No boundary checks are performed for performance reasons
        @param data The data to be programmed
        @param address The start address of the page
    */
    void writePage(uint16_t address, uint8_t* data);

    void writeBytes(uint16_t address, uint8_t* data, uint16_t size);
}