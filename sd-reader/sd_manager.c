/*
  Includes code from the LUFA Library, with the following
  copyright information:

  Copyright 2014  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

#include "sd_manager.h"
#include "sd_raw.h"
#include "scsi.h"

/** Buffer used in reading and writing */
static uint8_t sd_buffer[16];

/* Callback to read a single chunk of a block from the SD card into the
 * endpoint, processing 16 bytes.
 */
static uint8_t sd_read_block_handler(uint8_t* buffer, offset_t offset, void* p)
{
    /* Check if the endpoint is currently full */
    if (!(Endpoint_IsReadWriteAllowed()))
    {
        /* Clear the endpoint bank to send its contents to the host */
        Endpoint_ClearIN();

        /* Wait until the endpoint is ready for more data */
        if (Endpoint_WaitUntilReady())
          return 0;
    }

    //uint32_t top = offset >> 32;
    //uint32_t bottom = offset & 0xffffffff;
    //printf("0x%lx%lx:  ", top, bottom);
    //for (int i=0; i < 16; ++i)
        //printf("%02x ", buffer[i]);
    //printf("\n");

    Endpoint_Write_Stream_LE(buffer, 16, NULL);

    /* Check if the current command is being aborted by the host */
    if (((USB_ClassInfo_MS_Device_t*)p)->State.IsMassStoreReset)
      return 0;

    return 1;
}



void sd_read_blocks(USB_ClassInfo_MS_Device_t* const MSInterfaceInfo,
                    offset_t BlockAddress, uint16_t TotalBlocks)
{
    /* Wait until endpoint is ready before continuing */
    if (Endpoint_WaitUntilReady())
        return;

    //serial_wait();
    uint32_t top = BlockAddress >> 32;
    uint32_t bottom = BlockAddress & 0xffffffff;
    //printf("Reading %u blocks from 0x%lx%lx\n", TotalBlocks, top, bottom);
    //serial_wait();
    const offset_t EndAddress = BlockAddress + TotalBlocks;
    for (offset_t addr = BlockAddress; addr < EndAddress; addr++)
    {
        /* Read this block into the endpoint in 16-byte chunks. */
        sd_raw_read_interval(addr * VIRTUAL_MEMORY_BLOCK_SIZE,
                             sd_buffer, 16, VIRTUAL_MEMORY_BLOCK_SIZE,
                             &sd_read_block_handler, MSInterfaceInfo);
    }

    /* If the endpoint is full, send its contents to the host */
    if (!(Endpoint_IsReadWriteAllowed()))
      Endpoint_ClearIN();
}

////////////////////////////////////////////////////////////////////////////////

/* Callback to get data from the endpoint in preparation for writing it
 * to the SD card.  Handles 16 bytes of data at a time.
 */
static uintptr_t sd_write_block_handler(uint8_t* buffer, offset_t offset, void* p)
{
    /* Check if the endpoint is currently empty */
    if (!(Endpoint_IsReadWriteAllowed()))
    {
        /* Clear the current endpoint bank */
        Endpoint_ClearOUT();

        /* Wait until the host has sent another packet */
        if (Endpoint_WaitUntilReady())
          return 0;
    }

    /* Read in 16 bytes and store it in the buffer, from where it
     * will be loaded onto the SD card. */
    Endpoint_Read_Stream_LE(buffer, 16, NULL);

    /* Check if the current command is being aborted by the host */
    if (((USB_ClassInfo_MS_Device_t*)p)->State.IsMassStoreReset)
      return 0;

    return 16;
}

void sd_write_blocks(USB_ClassInfo_MS_Device_t* const MSInterfaceInfo,
                     offset_t BlockAddress, uint16_t TotalBlocks)
{
    /* Wait until endpoint is ready before continuing */
    if (Endpoint_WaitUntilReady())
      return;

    const offset_t EndAddress = BlockAddress + TotalBlocks;
    for (offset_t addr = BlockAddress; addr < EndAddress; addr++)
    {
        /* Read this block into the endpoint in 16-byte chunks. */
        sd_raw_write_interval(addr << VIRTUAL_MEMORY_BLOCK_SHIFT,
                              sd_buffer, VIRTUAL_MEMORY_BLOCK_SIZE,
                              &sd_write_block_handler, MSInterfaceInfo);
    }

    /* If the endpoint is empty, clear it ready for the next packet from the host */
    if (!(Endpoint_IsReadWriteAllowed()))
      Endpoint_ClearOUT();
}

////////////////////////////////////////////////////////////////////////////////

uint32_t sd_get_blocks(void)
{
    struct sd_raw_info info;
    sd_raw_get_info(&info);

    return info.capacity >> VIRTUAL_MEMORY_BLOCK_SHIFT;
}
