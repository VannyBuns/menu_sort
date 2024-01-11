#include <string.h>
#include <malloc.h>
#include <math.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "common/common.h"
#include <dirent.h>
#include <mocha/mocha.h>
#include <coreinit/filesystem_fsa.h>
#include <coreinit/ios.h>
#include <coreinit/mcp.h>
#include <coreinit/exit.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <coreinit/screen.h>
#include <coreinit/systeminfo.h>
#include <vpad/input.h>
#include <nn/ac.h>
#include <nn/act.h>
#include <unistd.h>
#include <wut.h>
#include <whb/log.h>
#include <whb/log_udp.h>
#include <whb/log_console.h>
#include <whb/proc.h>
#include "utils/dict.h"
#include "utils/file.h"

#define TITLE_TEXT "Sort Wii U Menu v1.3.0 - doino-gretchenliev & VannyBuns"
#define HBL_TITLE_ID 0x13374842
#define MAX_ITEMS_COUNT 300
#define _SYSGetSystemApplicationTitleId
#define OSScreenClearBuffer(color) ({ \
    OSScreenClearBufferEx(SCREEN_TV, color); \
    OSScreenClearBufferEx(SCREEN_DRC, color); \
})	

#define OSScreenPutFont(row, column, buffer) ({ \
    OSScreenPutFontEx(SCREEN_TV, row, column, buffer); \
    OSScreenPutFontEx(SCREEN_DRC, row, column, buffer); \
})

static const char *dontmovePath = "sd:/wiiu/apps/menu_sort/dontmove";
static const char *gamemapPath = "sd:/wiiu/apps/menu_sort/titlesmap";
static const char *backupPath = "sd:/wiiu/apps/menu_sort/BaristaAccountSaveFile.dat";

int badNamingMode = 0;
int ignoreThe = 0;
int backup = 0;
int restore = 0;
int count = 0;
struct MenuItemStruct
{
	uint32_t ID;
	uint32_t type;
	uint32_t titleIDPrefix;
	char name[65];
};

enum itemTypes
{
	MENU_ITEM_NAND = 0x01,
	MENU_ITEM_USB = 0x02,
	MENU_ITEM_DISC = 0x05,
	MENU_ITEM_VWII = 0x09,
	MENU_ITEM_FLDR = 0x10
};

	int startsWith(const char *a, const char *b);
    char *prepend(char *s, const char *t);
    void removeThe(char *src);


int smartStrcmp(const char *a, const char *b, const uint32_t a_id, const uint32_t b_id)
{
	char *ac = malloc(strlen(a) + 1);
	char *bc = malloc(strlen(b) + 1);

	strcpy(ac, a);
	strcpy(bc, b);

	if (ignoreThe)
	{
		removeThe(ac);
		removeThe(bc);
	}

	if (badNamingMode)
	{
		int a_id_size = (int)((ceil(log10(a_id)) + 1) * sizeof(char));
		char a_id_string[a_id_size];

		int b_id_size = (int)((ceil(log10(b_id)) + 1) * sizeof(char));
		char b_id_string[b_id_size];

		itoa(a_id, a_id_string, 16);
		itoa(b_id, b_id_string, 16);

		struct nlist *np;
		np = lookup(a_id_string);
		if (np != NULL)
		{
			ac = prepend(ac, np->defn);
		}

		np = lookup(b_id_string);
		if (np != NULL)
		{
			bc = prepend(bc, np->defn);
		}
	}

	int result = strcasecmp(ac, bc);
	free(ac);
	free(bc);
	return result;
}

int fSortCond(const void *c1, const void *c2)
{
	return smartStrcmp(((struct MenuItemStruct *)c1)->name,
					   ((struct MenuItemStruct *)c2)->name,
					   ((struct MenuItemStruct *)c1)->ID,
					   ((struct MenuItemStruct *)c2)->ID);
}

int readToBuffer(char **ptr, size_t *bufferSize, const char *path)
{
	FILE *fp;
	size_t size;
	fp = fopen(path, "rb");
	if (!fp)
	return -1;
	fseek(fp, 0L, SEEK_END); // fstat.st_size returns 0, so we use this instead
	size = ftell(fp);
	rewind(fp);
	*ptr = malloc(size);
	memset(*ptr, 0, size);
	fread(*ptr, 1, size, fp);
	fclose(fp);
	*bufferSize = size;
	return 0;
}

void getIDname(uint32_t id, uint32_t titleIDPrefix, char *name, size_t nameSize, uint32_t type)
{
	char path[255] = "";
	name[0] = 0;
	sprintf(path, "storage_%s:/usr/title/%08x/%08x/meta/meta.xml", (type == MENU_ITEM_USB) ? "usb" : "mlc", titleIDPrefix, id);
	WHBLogPrintf("%s\n", path);
}

int main(int argc, char **argv) {

    if (Mocha_InitLibrary() != MOCHA_RESULT_SUCCESS) {
        WHBLogPrintf("Mocha_InitLibrary failed");
        WHBLogConsoleDraw();
        OSSleepTicks(OSMillisecondsToTicks(3000));
        goto exit;
    }

	char *fBuffer = NULL;
	size_t fSize = 0;
	char failError[65] = "";
	int userPersistentId = 0;
	int screenGetPrintLine();
	void screenSetPrintLine(int line);
	int screenPrintAt(int x, int y, char *fmt, ...);
	uint32_t sysmenuId = 0;

	VPADInit();

	OSScreenInit();
	OSScreenClearBuffer(0);
	OSScreenPutFont(0, 0, TITLE_TEXT);
	OSScreenPutFont(0, 1, "Choose sorting method:");
	OSScreenPutFont(0, 2, "Press 'B' for standard sorting.");
	OSScreenPutFont(0, 3, "Press 'A' for standard sorting(ignoring leading 'The').");
	OSScreenPutFont(0, 4, "Press 'X' for bad naming mode sorting.");
	OSScreenPutFont(0, 5, "Press 'Y' for bad naming mode sorting(ignoring leading 'The').");
	OSScreenPutFont(0, 6, "Press '+' for backup of the current order(incl. folders).");
	OSScreenPutFont(0, 7, "Press '-' for restore of the current order(incl. folders).");
	OSScreenPutFont(0, 8, "Press 'L' for counting the items only(no changes).");


	char modeText[25] = "";
	char ignoreTheText[25] = "";

	WHBProcInit();
	VPADStatus buffer;
	while (WHBProcIsRunning())
	{

		VPADRead(VPAD_CHAN_0, &buffer, 1, NULL);
		uint32_t pressedBtns = 0;
			pressedBtns = (buffer.trigger | buffer.hold);

		if (pressedBtns & VPAD_BUTTON_B)
		{
			badNamingMode = 0;
			ignoreThe = 0;
			strcpy(modeText, "standard sorting");
			break;
		}

		if (pressedBtns & VPAD_BUTTON_A)
		{
			badNamingMode = 0;
			ignoreThe = 1;
			strcpy(modeText, "standard sorting");
			strcpy(ignoreTheText, "(ignoring leading 'The')");
			break;
		}

		if (pressedBtns & VPAD_BUTTON_X)
		{
			badNamingMode = 1;
			ignoreThe = 0;
			strcpy(modeText, "bad naming mode sorting");
			break;
		}

		if (pressedBtns & VPAD_BUTTON_Y)
		{
			badNamingMode = 1;
			ignoreThe = 1;
			strcpy(modeText, "bad naming mode sorting");
			strcpy(ignoreTheText, "(ignoring leading 'The')");
			break;
		}

		if (pressedBtns & VPAD_BUTTON_PLUS)
		{
			backup = 1;
			strcpy(modeText, "backup");
			strcpy(ignoreTheText, "");
			break;
		}

		if (pressedBtns & VPAD_BUTTON_MINUS)
		{
			restore = 1;
			strcpy(modeText, "restore");
			strcpy(ignoreTheText, "");
			break;
		}

		if (pressedBtns & VPAD_BUTTON_L)
		{
			count = 1;
			strcpy(modeText, "count");
			break;
		}

		usleep(1000);
	}

	OSScreenClearBuffer(0);
	void screenSetPrintLine(int line);
	OSScreenPutFont(0,0, TITLE_TEXT);
	char modeSelectedText[50] = "";
	sprintf(modeSelectedText, "Starting %s%s...", modeText, ignoreTheText);
	OSScreenPutFont(0, 2, modeSelectedText);
	WHBLogPrintf(modeSelectedText);

	// Get Wii U Menu Title
	// Do this before mounting the file system.
	// It screws it up if done after.
	uint64_t sysmenuIdUll = _SYSGetSystemApplicationTitleId(0);
	if ((sysmenuIdUll & 0xffffffff00000000) == 0x0005001000000000)
		sysmenuId = sysmenuIdUll & 0x00000000ffffffff;
	else
	{
		strcpy(failError, "Failed to get Wii U Menu Title!");
		goto exit;
	}
	WHBLogPrintf("Wii U Menu Title = %08X\n", sysmenuId);

	// Get current user account slot
	ACInitialize();
	
	int GetSlotNo();
	int GetPersistentId();
	unsigned char userSlot = GetSlotNo();
	userPersistentId = GetPersistentId(userSlot);
	WHBLogPrintf("User Slot = %d, ID = %08x\n", userSlot, userPersistentId);
	ACFinalize();

	//!*******************************************************************
	//!                        Initialize FS                             *
	//!*******************************************************************
	WHBLogPrintf("Mount partitions\n");

			FSAInit();
			int	gClient = FSAAddClient(NULL);
			if (gClient == 0) {
			WHBLogPrintf("Failed to add FSAClient");
			WHBLogConsoleDraw();
			OSSleepTicks(OSMillisecondsToTicks(3000));
			goto exit;
		}
		if (Mocha_UnlockFSClientEx(gClient) != MOCHA_RESULT_SUCCESS) 
		{
			FSADelClient(gClient);
			strcpy(failError, "Failed to add FSAClient");
			WHBLogConsoleDraw();
			OSSleepTicks(OSMillisecondsToTicks(3000));
			goto exit;
		}
		if (Mocha_MountFS("storage_slc", NULL, "/vol/system") < 0)
		{
			FSADelClient(gClient);
			strcpy(failError, "Failed to mount SLC!");
			WHBLogConsoleDraw();
			OSSleepTicks(OSMillisecondsToTicks(3000));
			goto exit;
		}
		if (Mocha_MountFS("storage_mlc", NULL, "/vol/storage_mlc01") < 0)
		{
			FSADelClient(gClient);
			strcpy(failError, "Failed to Mount MLC!");
			WHBLogConsoleDraw();
			OSSleepTicks(OSMillisecondsToTicks(3000));
			goto exit;
		}
		Mocha_MountFS("storage_usb", NULL, "/vol/storage_usb01");

	// Read Don't Move titles.
	// first try dontmoveX.hex where X is the user ID
	// if that fails try dontmove.txt
	char dmPath[65] = "";
	uint32_t *dmItem = NULL;
	int dmTotal = 0;
	size_t titleIDSize = 8;
	sprintf(dmPath, "%s%1x.txt", dontmovePath, userPersistentId & 0x0000000f);
	 FILE *fp = fopen(dmPath, "rb");
	if (!fp)
	{
		sprintf(dmPath, "%s.txt", dontmovePath);
		fp = fopen(dmPath, "rb");
	}
	if (fp)
	{
		sprintf("Loading %s\n", dmPath);
		int ch, lines = 0;
		do
		{
			ch = fgetc(fp);
			if (ch == '\n')
				lines++;
		} while (ch != EOF);
		rewind(fp);

		dmTotal = lines;
		dmItem = (uint32_t *)malloc(sizeof (uint32_t) * lines);

		WHBLogPrintf("Number of games to load(dontmove): %d\n", lines);

		for (int i = 0; i < lines; i++)
		{
			size_t len = titleIDSize;
			char *currTitleID = (char *)malloc(len + 1);
			getline(&currTitleID, &len, fp);
			dmItem[i] = (uint32_t)strtol(currTitleID, NULL, 16);
			free(currTitleID);
		}

		fclose(fp);
	}
	else
	{
		WHBLogPrintf("Could not open %s\n", dmPath);
	}

	if (badNamingMode)
	{
		char gmPath[65] = "";
		sprintf(gmPath, "%s%1x.psv", gamemapPath, userPersistentId & 0x0000000f);
		FILE *fp = fopen(gmPath, "rb");
		if (!fp)
		{
			sprintf(gmPath, "%s.psv", gamemapPath);
			fp = fopen(gmPath, "rb");
		}
		if (fp)
		{
			WHBLogPrintf("Loading %s\n", gmPath);
			int ch, max_ch_count, ch_count, lines = 0;
			do
			{
				ch = fgetc(fp);
				ch_count++;
				if (ch == '\n')
				{
					lines++;
					if (ch_count > max_ch_count)
						max_ch_count = ch_count;
				}
			} while (ch != EOF);
			rewind(fp);

			WHBLogPrintf("Number of games in the map to load(dontmove): %d\n", lines);

			for (int i = 0; i < lines; i++)
			{
				size_t len = max_ch_count;
				char *currTitleID = (char *)malloc(len + 1);
				getline(&currTitleID, &len, fp);

				char *id = strtok(currTitleID, "|");
				char *group = strtok(NULL, "|");
				group = strtok(group, "\n");
				install(id, group);
				free(currTitleID);
			}

			fclose(fp);
		}
		else
		{
			WHBLogPrintf("Could not open %s\n", gmPath);
		}
	}

	// Read Menu Data
	struct MenuItemStruct menuItem[300];
	bool folderExists[61] = {false};
	bool moveableItem[300];
	char baristaPath[255] = "";
	folderExists[0] = true;
	sprintf(baristaPath, "storage_mlc:/usr/save/00050010/%08x/user/%08x/BaristaAccountSaveFile.dat", sysmenuId, userPersistentId);
	WHBLogPrintf("%s\n", baristaPath);

	int itemsCount = 0;

	if (backup)
	{
		fcopy(baristaPath, backupPath);
	}
	else if (restore)
	{
		fcopy(backupPath, baristaPath);
	}
	else
	{
		if (readToBuffer(&fBuffer, &fSize, baristaPath) < 0)
		{
			strcpy(failError, "Could not open BaristaAccountSaveFile.dat\n");
			goto exit;
		}
		if (fBuffer == NULL)
		{
			strcpy(failError, "Memory not allocated for BaristaAccountSaveFile.dat\n");
			goto exit;
		}

		WHBLogPrintf("BaristaAccountSaveFile.dat size = %d\n", fSize);
		// Main Menu - First pass - Get names
		// Only movable items will be added.
		for (int fNum = 0; fNum <= 60; fNum++)
		{
			if (!folderExists[fNum])
				continue;
			WHBLogPrintf("\nReading - Folder %d\n", fNum);
			int currItemNum = 0;
			int movableItemsCount = 0;
			int maxItemsCount = MAX_ITEMS_COUNT;
			int folderOffset = 0;
			if (fNum != 0)
			{
				// sort sub folder
				maxItemsCount = 60;
				folderOffset = 0x002D24 + ((fNum - 1) * (60 * 16 * 2 + 56));
			}
			int usbOffset = maxItemsCount * 16;
			for (int i = 0; i < maxItemsCount; i++)
			{
				moveableItem[i] = true;
				int itemOffset = i * 16 + folderOffset;
				uint32_t id = 0;
				uint32_t type = 0;
				memcpy(&id, fBuffer + itemOffset + 4, sizeof (uint32_t));
				memcpy(&type, fBuffer + itemOffset + 8, sizeof(uint32_t));

				if ((id == HBL_TITLE_ID)		  // HBL
					|| (type == MENU_ITEM_DISC)	  // Disc
					|| (type == MENU_ITEM_VWII))  // vWii
				{
					moveableItem[i] = false;
					itemsCount++;
					continue;
				}
				// Folder item in main menu?
				if ((fNum == 0) && (type == MENU_ITEM_FLDR))
				{
					if ((id > 0) && (id <= 60))
						folderExists[id] = true;
					moveableItem[i] = false;
					continue;
				}
				// NAND or USB?
				if (type == MENU_ITEM_NAND)
				{
					// Settings, MiiMaker, etc?
					uint32_t idH = 0;
					memcpy(&idH, fBuffer + itemOffset, sizeof (uint32_t));

					if ((idH != 0x00050000) && (idH != 0x00050002) && (idH != 0))
					{
						moveableItem[i] = false;
						continue;
					}

					// If not NAND then check USB
					if (id == 0)
					{
						itemOffset += usbOffset;
						id = 0;
						memcpy(&id, fBuffer + itemOffset + 4, sizeof (uint32_t));
						type = fBuffer[itemOffset + 0x0b];
						if ((id == 0) || (type != MENU_ITEM_USB))
							continue;
					}

					itemsCount++;

					// Is ID on Don't Move list?
					for (int j = 0; j < dmTotal; j++)
					{
						if (id == dmItem[j])
						{
							moveableItem[i] = false;
							break;
						}
					}

					if (!moveableItem[i])
						continue;

					// found sortable item
					memcpy(&menuItem[currItemNum].titleIDPrefix, fBuffer + itemOffset, sizeof (uint32_t));
					getIDname(id, menuItem[currItemNum].titleIDPrefix, menuItem[currItemNum].name, 65, type);
					menuItem[currItemNum].ID = id;
					menuItem[currItemNum].type = type;
					currItemNum++;
				}
			}
			movableItemsCount = currItemNum;
			WHBLogPrintf("\nDone reading folders \n");

			if (!count)
			{
				// Sort Folder
				qsort(menuItem, movableItemsCount, sizeof(struct MenuItemStruct), fSortCond);

				// Move Folder Items
				WHBLogPrintf("\nNew Order - Folder %d\n", fNum);
				currItemNum = 0;
				for (int i = 0; i < maxItemsCount; i++)
				{
					if (!moveableItem[i])
						continue;
					int itemOffset = i * 16 + folderOffset;
					uint32_t idNAND = 0;
					uint32_t idNANDh = 0;
					uint32_t idUSB = 0;
					uint32_t idUSBh = 0;
					if (currItemNum < movableItemsCount)
					{
						if (menuItem[currItemNum].type == MENU_ITEM_NAND)
						{
							idNAND = menuItem[currItemNum].ID;
							idNANDh = menuItem[currItemNum].titleIDPrefix;
						}
						else
						{
							idUSB = menuItem[currItemNum].ID;
							idUSBh = menuItem[currItemNum].titleIDPrefix;
						}
						WHBLogPrintf("[%d][%08x] %s\n", i, menuItem[currItemNum].ID, menuItem[currItemNum].name);
						currItemNum++;
					}

					memcpy(fBuffer + itemOffset, &idNANDh, sizeof (uint32_t));
					memcpy(fBuffer + itemOffset + 4, &idNAND, sizeof (uint32_t));
					memset(fBuffer + itemOffset + 8, 0, 8);
					fBuffer[itemOffset + 0x0b] = 1;

					itemOffset += usbOffset;

					memcpy(fBuffer + itemOffset, &idUSBh, sizeof (uint32_t));
					memcpy(fBuffer + itemOffset + 4, &idUSB, sizeof (uint32_t));
					memset(fBuffer + itemOffset + 8, 0, 8);
					fBuffer[itemOffset + 0x0b] = 2;
				}
			}
		}

		if (!count)
		{
			FILE *fp = fopen(baristaPath, "wb");
			if (fp)
			{
				fwrite(fBuffer, 1, fSize, fp);
				fclose(fp);
			}
			else
			{
				strcpy(failError, "Could not write to BaristaAccountSaveFile.dat\n");
				goto exit;
			}
		}

		free(fBuffer);
		free(dmItem);
	}

	screenPrintAt(strlen(modeSelectedText), screenGetPrintLine(), "done.");

	char text[20] = "";
	sprintf(text, "User ID: %1x", userPersistentId & 0x0000000f);
	OSScreenPutFont(0, 1, text);
	if (itemsCount != 0)
	{
		char countText[21] = "";
		sprintf(countText, "Items count: %d/%d", itemsCount, MAX_ITEMS_COUNT);
		OSScreenPutFont(0, 2, countText);
	}
	OSScreenPutFont(0, 4, "Press Home to exit");

	while (WHBProcIsRunning())
	{
		 VPADRead(VPAD_CHAN_0, &buffer, 1, NULL);
		uint32_t pressedBtns = 0;
			pressedBtns = buffer.trigger | buffer.hold;

		if (pressedBtns & VPAD_BUTTON_HOME)
			break;

		OSSleepTicks(OSMillisecondsToTicks(100));
	}

exit:
	WHBLogPrintf("Unmount\n");
	Mocha_UnmountFS("storage_slc");
	Mocha_UnmountFS("storage_mlc");
	Mocha_UnmountFS("storage_usb");
	Mocha_DeInitLibrary();
	WHBLogPrintf("Exiting\n");  
	//memoryRelease();
	WHBLogUdpDeinit();
	WHBLogConsoleFree();
	WHBProcShutdown();

	return 0;
}

/*******************************************************************************
 * 
 * Menu Title IDs
 * USA 00050010-10040100
 * EUR 00050010-10040200
 * JPN
 * 
 * USA Wii U Menu layout file location: (8000000X where X is the User ID)
 * storage_mlc\usr\save\00050010\10040100\user\8000000X\BaristaAccountSaveFile.dat
 * 
 * File Format:
 * 0x000000 - 0x0012BF  NAND Title Entries (300 x 16 bytes)
 * 0x0012C0 - 0x00257F  USB Title Entries  (300 x 16 bytes)
 * 0x002587 - Last ID Launched
 * 0x0025A4 - 0x002963  ? Folder 0 ? NAND Title Entries (60 x 16 bytes)
 * 0x002964 - 0x002D23  ? Folder 0 ? USB Title Entries  (60 x 16 bytes)
 * 0x002D24 - 0x0030E3  Folder 1 NAND Title Entries (60 x 16 bytes)
 * 0x0030E4 - 0x0030E3  Folder 1 USB Title Entries  (60 x 16 bytes)
 * 0x0034A4 - 0x0034DB  Folder 1 Info (56 Bytes)
 * 0x0034DC - 0x00389B  Folder 2 NAND Title Entries (60 x 16 bytes)
 * 0x00389C - 0x003C5B  Folder 2 USB Title Entries  (60 x 16 bytes)
 * 0x003C5C - 0x003C93  Folder 2 Info (56 Bytes)
 * 0X003C94 .... FOLDERS 3 - 60 AS ABOVE
 * 
 * Title Entry Data Format:
 * Each entry is 16 bytes.
 * 0x00 - 0x07  Title ID
 * 0x0B  Type:
 *  0x01 = NAND
 *  0x02 = USB
 *  0X05 = Disc Launcher
 *  0X09 = vWii (Not Eshop vWii titles)
 *  0x10 = Folder (Bytes 0x00-0x06 = 0, 0x07 = Folder#)
 * 
 * If a 16 byte entry is set in NAND the corresponding entry in USB is blank
 * (except for the type 0x02 at offset 0x0b) and vice-versa.
 * 
 * Folders are all in - 0x000000 - 0x0012BF NAND IDs.
 * USB entry would be blank (except for the type 0x02 at offset 0x0b)
 * 
 * Folder Entry Data Format:
 * 0x00 - 0x21  Name (17 Unicode characters)
 * 0x32         Color
 *  0x00 = Blue
 *  0x01 = Green
 *  0x02 = Yellow
 *  0x03 = Orange
 *  0x04 = Red
 *  0x05 = Pink
 *  0x06 = Purple
 *  0x07 = Grey
 * 
 ******************************************************************************/
