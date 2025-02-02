#include "SerialPort.h"

void SerialPort::writeSerialControl(uint8_t val)
{
    s.serial_control = val;
    if (s.serial_control & 0x80) 
    {
        s.serialCycles = 0;
        s.transferredBits = 0;
    }
}

uint8_t SerialPort::readSerialControl()
{
    // Bit 1 (high clock speed) is unused in DMG / DMG Compat mode.
    const uint8_t mask = System::Current() == GBSystem::CGB ? 0b01111100 : 0b01111110;
    return s.serial_control | mask;
}

void SerialPort::execute() 
{
    if (!(s.serial_control & 0x80)) // Transfer is disabled.
        return; 

    if (s.serial_control & 0x1) // For now executing only if internal clock is selected.
        s.serialCycles++;

    const bool highClockSpeed = System::Current() == GBSystem::CGB && (s.serial_control & 0b10);
    const uint16_t serialTransferCycles = highClockSpeed ? 128 : 4;

    if (s.serialCycles >= serialTransferCycles)
    {
        s.serialCycles -= serialTransferCycles;
        s.transferredBits++;
        s.serial_reg <<= 1;
        s.serial_reg |= 0x1; // For now 0x1 represents not connected. Later replace with actual value.

        if (s.transferredBits == 8)
        {
            s.serial_control &= 0x7F;
            cpu.requestInterrupt(Interrupt::Serial);
        }
    }
}