#include "device.h"
#include <stdio.h>
#include <stddef.h>
#include "pico/unique_id.h"
#include "pico/version.h"
#include "hardware/regs/addressmap.h"
#include "hardware/regs/sysinfo.h"



    char board_id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];
    volatile uint32_t *chip_id;
    uint32_t id;
    uint32_t manufacturer;
    uint32_t part;
    uint32_t revision;

       
    struct info_t device_card = {0x00010000,"es-cmd-usb",2};
   
    


void dev_info(void)
{
 
        printf("%-15s %-12s %-6s %-6s %6s\n", "struct", "address", "size", "offset", "value");

   
        printf("- %-13s 0x%08x %5u\n",
           "device_card",
           &device_card,
           sizeof(device_card));


        printf("- %-13s 0x%08x %5u %5u     0x%08x\n",
           "version",
           &device_card.version,
           sizeof(device_card.version),
           offsetof(struct info_t, version),
           device_card.version);
    

        printf("- %-13s 0x%08x %5u %5u     %-13s\n",
           "name",
           device_card.name,
           sizeof(device_card.name),
           offsetof(struct info_t, name),
           device_card.name);

        printf("- %-13s 0x%08x %5u %5u %5u\n",
           "revision",
           &device_card.revision,
           sizeof(device_card.revision),
           offsetof(struct info_t, revision),
           device_card.revision);


           unsigned temp_sizeof = sizeof(device_card);
           unsigned fields = sizeof(device_card.version) + sizeof(device_card.name) + sizeof(device_card.revision);


        printf("fields %3u, of sizeof %3u, padding %3u\n",
           fields,
           temp_sizeof,
           temp_sizeof - fields);
}


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
    printf("pico-sdk: %s\n", PICO_SDK_VERSION_STRING);

}

