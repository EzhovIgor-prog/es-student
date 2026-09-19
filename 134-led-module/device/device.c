#include "device.h"
#include <stdio.h>
#include "pico/unique_id.h"
#include "hardware/regs/addressmap.h"
#include "hardware/regs/sysinfo.h"



    char board_id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];
    volatile uint32_t *chip_id;
    uint32_t id;
    uint32_t manufacturer;
    uint32_t part;
    uint32_t revision;

//volatile uint32_t chip_id = (uint32_t *)(SYSINFO_BASE + SYSINFO_CHIP_ID_OFFSET);



//uint32_t manufacturer;
//uint32_t part;
//uint32_t revision;


void device_info(void)
{
    

    chip_id = (uint32_t *)(SYSINFO_BASE + SYSINFO_CHIP_ID_OFFSET);
    uint32_t id = *chip_id;

    manufacturer = (id & SYSINFO_CHIP_ID_MANUFACTURER_BITS) >> SYSINFO_CHIP_ID_MANUFACTURER_LSB;
    part = (id & SYSINFO_CHIP_ID_PART_BITS) >> SYSINFO_CHIP_ID_PART_LSB;
    revision = (id & SYSINFO_CHIP_ID_REVISION_BITS) >> SYSINFO_CHIP_ID_REVISION_LSB;

    
    pico_get_unique_board_id_string(board_id, sizeof(board_id));

    printf("project: %s\n", DEVICE_PROJECT);
    printf("repo: %s\n", DEVICE_REPO);
    printf("board: %s\n", DEVICE_BOARD);
    printf("serial: %s\n", board_id);
    printf("chip: manufacturer 0x%03x, part 0x%04x, revision %u\n", manufacturer, part, revision);

}

