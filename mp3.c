#include <stdio.h>
#include <avr/io.h>

#include "mp3.h"
#include "macros.h"

int mp3_init()
{
    // MP3 chip select
    OUTPUT(DDRF, PF4);
    SET(PORTF, PF4);

    // MP3 data select
    OUTPUT(DDRF, PF1);
    SET(PORTF, PF1);

    // MP3 chip reset
    OUTPUT(DDRF, PF5);
    SET(PORTF, PF5);

    // All of the spi stuff is configured by the SD card,
    // so we don't need to do anything here.

    // Check to make sure that this chip is the right one.
    const uint16_t version = (mp3_read(0x1) & 0xf0) >> 4;
    if (version != 3) {
        printf("Error: Unexpected VS1003ds version (%i)", version);
        return 0;
    }

    return 1;
}

void mp3_select()
{
    CLEAR(PORTF, PF4);
}

void mp3_deselect()
{
    SET(PORTF, PF4);
}

void mp3_data_select()
{
    CLEAR(PORTF, PF1);
}

void mp3_data_deselect()
{
    SET(PORTF, PF1);
}

uint8_t mp3_spi_send(const uint8_t b)
{
    SPDR = b;
    while(!(SPSR & (1 << SPIF)));
    CLEAR(SPSR, SPIF);
    return SPDR;
}

void mp3_wait()
{
    // Wait for DREQ to go high (signaling that the mp3 chip
    // can take in an SPI command).
    while(!(PINF & (1 << PF6)));
}


uint16_t mp3_read(const uint8_t addr)
{
    mp3_wait();

    // Turn SPI frequency doubling off
    SPSR &= ~(1 << SPI2X);

    // Select the mp3 for a data transmission
    mp3_select();

    mp3_spi_send(0b00000011); // opcode for read
    mp3_spi_send(addr);

    // Use dummy sends to get out data
    uint16_t out = 0;
    out = mp3_spi_send(0);
    out <<= 8;
    out = mp3_spi_send(0);

    mp3_deselect();

    // Turn SPI frequency doubling back on
    SPSR |= (1 << SPI2X);

    return out;
}

void mp3_write(const uint8_t addr, const uint16_t data)
{
    mp3_wait();
    SPSR &= ~(1 << SPI2X); // turn off SPI frequency doubling
    mp3_select();

    mp3_spi_send(0b00000010); // opcode for write
    mp3_spi_send(addr);
    mp3_spi_send(data >> 8);
    mp3_spi_send(data & 0xff);

    mp3_deselect();
    SPSR |= (1 << SPI2X); // turn SPI frequency doubling back on
}
