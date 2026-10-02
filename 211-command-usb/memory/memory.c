#include "memory.h"
#include "command.h"
#include <stdlib.h>
#include "device.h"




    extern char __flash_binary_start;
    extern char __flash_binary_end;
    extern char __boot2_start__;
    extern char __boot2_end__;
    extern char __etext;
    extern char __data_start__;
    extern char __data_end__;
    extern char __bss_start__;
    extern char __bss_end__;
    extern char __HeapLimit;
    extern char __StackBottom;
    extern char __StackTop;

    extern int main(void);
   
    uint32_t data_variable = 100;
    uint32_t bss_variable;



    static void row(const char *name, uintptr_t start, uintptr_t end)
    {
        printf("%-10s 0x%08x 0x%08x %-8u\n",
            name, (unsigned)start, (unsigned)end, (unsigned)(end - start));
    }

    static void rowint(const char *name, uint32_t start, uint32_t end)
    {
        printf("%-10s 0x%08x 0x%08x %-8u\n",
            name, start, end, (end - start));
    }

    //  flash image    38224 = boot2 256 + text 32528 + data 5440
    static void total_flashImg(uintptr_t summ, uintptr_t prm1, uintptr_t prm2, uintptr_t prm3)
    {
        printf("   flash image   %u  = boot2 %u  + text %u + data %u\n",
                (unsigned)summ, (unsigned)prm1, (unsigned)prm2, (unsigned)prm3);
    }

    //  flash free   2058928 of 2097152
    static void total_flashFree(uint32_t prm1, uint32_t prm2)
    {
        printf("   flash free    %u  of %u\n", prm1, prm2);
    }

    //  ram used        8948 = data 5440 + bss 3508
    static void total_ramUsed(uintptr_t summ, uintptr_t prm1, uintptr_t prm2)
    {
        printf("   ram used      %u  = data %u + bss %u\n", (unsigned)summ, (unsigned)prm1, (unsigned)prm2);
    }

    // ram free      252492 for heap and 2048 for stack
    static void total_ramFree(uintptr_t prm1, uintptr_t prm2)
    {
        printf("   ram free      %u for heap and %u for stack\n", (unsigned)prm1, (unsigned)prm2);
    }


    void mem_info(void)
    {
        // шапка таблицы: область, начало, конец, размер
        //printf("area       start      end        size\n");
        printf("%-10s %-10s %-6s %8s\n", "area", "start", "end", "size");

        uint32_t    mem1 = XIP_BASE + PICO_FLASH_SIZE_BYTES;
        // flash — XIP_BASE и PICO_FLASH_SIZE_BYTES
        //rowint("flash",  XIP_BASE, (XIP_BASE + PICO_FLASH_SIZE_BYTES));
        rowint("flash",  XIP_BASE, mem1);

        // sram — базовый адрес из SDK, размер из datasheet
        rowint("sram",  SRAM_BASE, (SRAM_BASE + (0x400*264)));

        // rom — базовый адрес из SDK, размер из datasheet
        rowint("rom",  ROM_BASE, (ROM_BASE + 0x4000));

        // image — от __flash_binary_start до __flash_binary_end
        row("image", (uintptr_t)&__flash_binary_start, (uintptr_t)&__flash_binary_end);

        // free  — от __flash_binary_end до конца флеш-памяти
        rowint("free",  (uint32_t)((void *)&__flash_binary_end), mem1);

        // boot2 — от __boot2_start__ до __boot2_end__
        row("boot2", (uintptr_t)&__boot2_start__, (uintptr_t)&__boot2_end__);

        // text  — от __boot2_end__ до __etext: код и константы
        row("text", (uintptr_t)&__boot2_end__, (uintptr_t)&__etext);

        //uintptr_t data_flash_size  = (uintptr_t)&__data_end__ - (uintptr_t)&__data_start__;

        // data flash — хранение .data, от __etext, длиной с .data
        row("data flash", (uintptr_t)&__etext, (uintptr_t)&__etext + ((uintptr_t)&__data_end__ - (uintptr_t)&__data_start__));

        // data ram   — работа .data, от __data_start__ до __data_end__
        row("data ram", (uintptr_t)&__data_start__, (uintptr_t)&__data_end__);

        // bss        — от __bss_start__ до __bss_end__
        row("bss", (uintptr_t)&__bss_start__, (uintptr_t)&__bss_end__);

        // heap       — от __bss_end__ до __HeapLimit
        row("heap", (uintptr_t)&__bss_end__, (uintptr_t)&__HeapLimit);

        // stack      — от __StackBottom до __StackTop
    row("stack", (uintptr_t)&__StackBottom, (uintptr_t)&__StackTop);



        printf("\ntotal\n");
        
        total_flashImg(((uintptr_t)&__boot2_end__ - (uintptr_t)&__boot2_start__) + \
                        ((uintptr_t)&__etext - (uintptr_t)&__boot2_end__) +\
                        ((uintptr_t)&__data_end__ - (uintptr_t)&__data_start__),\
                        ((uintptr_t)&__boot2_end__ - (uintptr_t)&__boot2_start__),\
                        ((uintptr_t)&__etext - (uintptr_t)&__boot2_end__),\
                        ((uintptr_t)&__data_end__ - (uintptr_t)&__data_start__));


        total_flashFree((PICO_FLASH_SIZE_BYTES - ((uint32_t)((void *)&__flash_binary_end) - XIP_BASE)), PICO_FLASH_SIZE_BYTES);

        total_ramUsed(((uintptr_t)&__data_end__ - (uintptr_t)&__data_start__) +\
                    ((uintptr_t)&__bss_end__ - (uintptr_t)&__bss_start__),\
                    (uintptr_t)&__data_end__ - (uintptr_t)&__data_start__,\
                    (uintptr_t)&__bss_end__ - (uintptr_t)&__bss_start__);


        total_ramFree((uintptr_t)&__HeapLimit - (uintptr_t)&__bss_end__,\
                    (uintptr_t)&__StackTop - (uintptr_t)&__StackBottom);

    }

    void fw_info(void)
    {

        data_variable++;
        bss_variable++;

      


    //heap_variable = (uint32_t*)(malloc(sizeof(uint32_t)));

     // считаем вызов: data_variable и bss_variable на единицу больше
    // адреса функций со сброшенным признаком Thumb
    // локальная переменная и блок из кучи

    // шапка: объект, адрес, значение
    printf("%-13s   %-8s   %8s\n", "object", "address", "value");
    
    
    // main, fw_info  — адрес с признаком Thumb и два байта по сброшенному адресу
    uintptr_t code_addr = (uintptr_t)main & ~(uintptr_t)1;  // адрес первой команды: младший разряд сброшен
    uint16_t first = *(uint16_t *)code_addr;    // первая команда функции: 
    printf("%-10s      0x%08x   0x%04x\n", "main",code_addr, first);

    
    code_addr = (uintptr_t)fw_info & ~(uintptr_t)1; // адрес первой команды: младший разряд сброшен
    first = *(uint16_t *)code_addr; // первая команда функции: 
    printf("%-10s      0x%08x   0x%04x\n", "fw_info", code_addr, first);

    // commands       — адрес массива
    code_addr = (uintptr_t)commands;
    printf("%-10s      0x%08x\n", "commands", code_addr);

  
  // обработчики    — имя команды и адрес обработчика, строкой на команду
    for (uint i = 0; i < command_count; i++)
    {
         code_addr = (uintptr_t)commands[i].handler & ~(uintptr_t)1;
        printf("- %-10s    0x%08x\n", commands[i].name, (unsigned)code_addr );
    }
    
    // константы      — адрес и значение строк паспорта из device.h
    char *char_addr;
    char_addr = (char*)DEVICE_PROJECT;
    printf("DEVICE_PROJECT  0x%08x %s\n", char_addr, DEVICE_PROJECT);

    char_addr = (char*)DEVICE_BOARD;
    printf("DEVICE_BOARD    0x%08x %s\n", char_addr, DEVICE_BOARD);

    //uint32_t *pTemp;
    //pTemp = (uint32_t*) data_variable;
    uint32_t *pTemp = &data_variable;
    printf("data_variable   0x%08x %u\n", pTemp, *pTemp);

    
    uint32_t *pTemp1 = &bss_variable;
    printf("bss_variable    0x%08x %u\n", pTemp1, *pTemp1);

        uint32_t *ptr2;
        uint32_t stack_variable = 1946;
        ptr2 = &stack_variable;

        uint32_t *heap_variable = malloc(sizeof(uint32_t));

        if (heap_variable != NULL)
        {
            *heap_variable = 1951;
        }

        printf("stack_variable   0x%08x %u\n", ptr2, stack_variable);
        printf("heap_variable    0x%08x %u\n", heap_variable, *heap_variable);
    
    
    // data_variable  — адрес и значение, секция .data
    // bss_variable   — адрес и значение, секция .bss
    // stack_variable — адрес и значение
    // heap_variable  — адрес и значение

    // возвращаем блок кучи
    free(heap_variable);
    }