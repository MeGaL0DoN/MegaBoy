#include "SerialPort.h"

void SerialPort::writeSerialControl(uint8_t val)
{
    const bool transferEnabled { ((val & 0x80) && !(s.serial_control & 0x80)) };
    const bool clockSpeedChanged { System::Current() == GBSystem::CGB && ((val & 0b10) != (s.serial_control & 0b10)) };

    if (transferEnabled || clockSpeedChanged) 
    {
        s.serialCycles = 0;
        s.transferredBits = 0;
    }

    s.serial_control = val;
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

    if (!(s.serial_control & 0x1)) // External clock is selected.
        return;

    const bool highClockSpeed { System::Current() == GBSystem::CGB && (s.serial_control & 0b10) };
    const int serialTransferCycles { highClockSpeed ? 128 : 4 };

    if (++s.serialCycles >= serialTransferCycles)
    {
        s.serialCycles -= serialTransferCycles;
        s.transferredBits++;
        s.serial_reg <<= 1;
        s.serial_reg |= 0x1; // For now 0x1 represents not connected. Later replace with actual value.

        if (s.transferredBits == 8)
        {
            s.serial_control &= (~0x80);
            cpu.requestInterrupt(Interrupt::Serial);
        }
    }
}