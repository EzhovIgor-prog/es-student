#include "block-task/block-task.h"
#include <stddef.h>
#include "platform/platform.h"


static block_task_callback_t block_callback;

uint32_t block_counter;

void block_task_handle(void)
{
    if(platform_block_done() == false ) return;

    platform_erase_fDone(); 
   
     
    block_callback(platform_block_samples(), block_counter, platform_block_time_us());
    block_callback = NULL;
}



bool block_task_start(uint32_t count, block_task_callback_t on_block)
{
    

    if((on_block == NULL) || (!platform_block_start(count))) return false;

    block_callback = on_block;
    block_counter = count;

    return true;
}


