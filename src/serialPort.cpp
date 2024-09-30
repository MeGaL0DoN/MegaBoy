#include "serialPort.h"
#include "defines.h"

void serialPort::writeSerialControl(uint8_t val)
{
    s.serial_control = val | 0b01111100;
    if (s.serial_control & 0x80) 
    {
        s.serialCycles = 0;
        s.transferredBits = 0;
    }
}

void serialPort::execute() 
{
    if (s.serial_control & 0x80)
    {
        if (s.serial_control & 0x1) // For now executing only if internal clock is selected.
            s.serialCycles++;

        if (s.serialCycles >= SERIAL_TRANSFER_CYCLES)
        {
            s.serialCycles -= SERIAL_TRANSFER_CYCLES;
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
}

void serialPort::saveState(std::ofstream& st)
{
    ST_WRITE(s);
}

void serialPort::loadState(std::ifstream& st)
{
    ST_READ(s);
}