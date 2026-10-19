#include "stdio-text-protocol/stdio-text-protocol.h"

#include <stddef.h>
#include <stdio.h>
#include "pico/stdlib.h"

static char line[STDIO_TEXT_PROTOCOL_LINE_SIZE];
static uint32_t stringLength;
static bool oversize;
static command_t command;

void stdio_text_protocol_init(void)
{
    stringLength = 0;
    oversize = false;
    command = (command_t){0};
}

static const command_t *split_line(void)
{
    command = (command_t){.truncated = oversize};
    char *cursor = line;
    while (*cursor != '\0')
    {
        if (*cursor == ' ')
        {
            *cursor++ = '\0';
            continue;
        }
        if (command.name == NULL)
        {
            command.name = cursor;
        }
        else if (command.argc < STDIO_TEXT_PROTOCOL_MAX_ARGS)
        {
            command.argv[command.argc++] = cursor;
        }
        else
        {
            command.truncated = true;
        }
        while (*cursor != '\0' && *cursor != ' ')
        {
            cursor++;
        }
    }
    return command.name != NULL || command.truncated ? &command : NULL;
}

const command_t *stdio_text_protocol_handle(void)
{
    int symbol = getchar_timeout_us(0);
    if (symbol == PICO_ERROR_TIMEOUT)
    {
        return NULL;
    }
    if (symbol == '\r' || symbol == '\n')
    {
        if (stringLength == 0 && !oversize)
        {
            return NULL;
        }
        line[stringLength] = '\0';
        printf("\r\n");
        const command_t *accepted = split_line();
        stringLength = 0;
        oversize = false;
        return accepted;
    }
    if (symbol == 0x08 || symbol == 0x7f)
    {
        if (stringLength != 0)
        {
            stringLength--;
            oversize = false;
            printf("\b \b");
        }
        return NULL;
    }
    if (symbol >= 0x20 && symbol <= 0x7e)
    {
        if (stringLength + 1 < STDIO_TEXT_PROTOCOL_LINE_SIZE)
        {
            line[stringLength++] = (char)symbol;
            putchar(symbol);
        }
        else
        {
            oversize = true;
        }
    }
    return NULL;
}
