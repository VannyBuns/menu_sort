#include <stdarg.h>
#include <stdio.h>
#include <malloc.h>
#include "screen.h"

static int current_line = 0;
int buf0_size, buf1_size;
extern int vasprintf(char **strp, const char *fmt, va_list ap);

void screenFlip()
{
    //Flip the buffer
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
}

void screenInit()
{
    OSScreenInit();
    //Grab the buffer size for each screen (TV and gamepad)
    buf0_size = OSScreenGetBufferSizeEx(SCREEN_TV);
    buf1_size = OSScreenGetBufferSizeEx(SCREEN_DRC);
 	//Set buffer to some free location in MEM1
    OSScreenSetBufferEx(SCREEN_TV, (char *)0xF4000000);
    OSScreenSetBufferEx(SCREEN_DRC, ((char *)0xF4000000 + buf0_size));
    //Enable the screen
    OSScreenEnableEx(SCREEN_TV, 1);
    OSScreenEnableEx(SCREEN_DRC, 1);
}

int screenGetPrintLine()
{
    return (current_line - 1);
}

void screenSetPrintLine(int line)
{
    current_line = line;
}

int screenPrintAt(int x, int y, const char *fmt, ...)
{
    char *tmp = NULL;
    va_list va;
    va_start(va, fmt);
    
    int result = vasprintf(&tmp, fmt, va);
    if (result >= 0 && tmp)
    {
        OSScreenPutFontEx(SCREEN_DRC, x, y, tmp);
        screenFlip();
        OSScreenPutFontEx(SCREEN_DRC, x, y, tmp);
        screenFlip();
    }
    else
    {
        // Handle error case
        fprintf(stderr, "Error in vasprintf\n");
    }

    va_end(va);
    if (tmp)
        free(tmp);

    return 0; // Return the result of vasprintf
}
