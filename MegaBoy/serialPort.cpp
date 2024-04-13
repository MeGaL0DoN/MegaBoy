#include "serialPort.h"
#include <iostream>

void serialPort::writeSerialReg(uint8_t val)
{
    serial_reg = val;
}
void serialPort::writeSerialControl(uint8_t val)
{
    serial_control = val;
    if (serial_control & 0x80) 
    {
        serialCycles = 0;
        transferredBits = 0;
    }
}

void serialPort::execute(uint8_t cycles) {
    if (serial_control & 0x80)
    {
        if (serial_control & 0x1) // For now executing only if internal clock is selected.
            serialCycles += cycles;

        if (serialCycles >= SERIAL_TRANSFER_CYCLES)
        {
            serialCycles -= SERIAL_TRANSFER_CYCLES;
            transferredBits++;
            serial_reg <<= 1;
            serial_reg |= 0x1; // For now 0x1 represents not connected. Later replace with actual value.

            if (transferredBits == 8)
            {
                serial_control &= 0x7F;
                cpu.requestInterrupt(Interrupt::Serial);
            }
        }
    }
}