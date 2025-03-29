#include "SerialPort.h"

void SerialPort::writeSerialControl(uint8_t val)
{
    const bool transferEnabled { ((val & 0x80) && !(s.serialControl & 0x80)) };
    const bool clockSpeedChanged { System::Current() == GBSystem::CGB && ((val & 0b10) != (s.serialControl & 0b10)) };

    if (transferEnabled || clockSpeedChanged) 
    {
        s.serialCycles = 0;
        s.transferredBits = 0;
    }

    s.serialControl = val;
}

uint8_t SerialPort::readSerialControl()
{
    // Bit 1 (high clock speed) is unused in DMG / DMG Compat mode.
    const uint8_t mask = System::Current() == GBSystem::CGB ? 0b01111100 : 0b01111110;
    return s.serialControl | mask;
}

void SerialPort::execute() 
{
    if (!(s.serialControl & 0x80)) // Transfer is disabled.
        return; 

    if (!(s.serialControl & 0x1)) // External clock is selected.
        return;

    const bool highClockSpeed { System::Current() == GBSystem::CGB && (s.serialControl & 0b10) };
    const int serialTransferCycles { highClockSpeed ? 128 : 4 };

    if (++s.serialCycles >= serialTransferCycles)
    {
        s.serialCycles -= serialTransferCycles;
        s.transferredBits++;
        s.serialReg <<= 1;
        s.serialReg |= 0x1; // For now 0x1 represents not connected. Later replace with actual value.

        if (s.transferredBits == 8)
        {
            s.serialControl &= (~0x80);
            cpu.requestInterrupt(Interrupt::Serial);
        }
    }
}