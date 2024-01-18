#pragma once
#include <wut.h>
#include <coreinit/screen.h>

#ifdef __cplusplus
extern "C"
{
#endif

	void screenFlip();
	int screenGetPrintLine();
	void screenSetPrintLine(int line);
	int screenPrintAt(int x, int y, const char *fmt, ...);
#ifdef __cplusplus
}
#endif