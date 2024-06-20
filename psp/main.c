#include <pspkernel.h>
#include <pspmodulemgr.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspthreadman.h>
#include <pspctrl.h>
#include <pspsdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shared.h"
#include "pspcommon.h"

//See DEBUG_MODE in menuplus.c, SMS.c
//#define KERNEL_MODE

#ifdef KERNEL_MODE
	PSP_MODULE_INFO("MasterBoyKernel", 0x1000, 0, 1); /* 0x1000 REQUIRED!!! */
	PSP_MAIN_THREAD_ATTR(THREAD_ATTR_VFPU);
#else
	PSP_MODULE_INFO("MasterBoy", 0, 0, 1);
	PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER|THREAD_ATTR_VFPU);
#endif

static char startPath[1024];
int gbUsbAvailable = 0;

#ifdef KERNEL_MODE
int exitCallback(int arg1, int arg2, void *common)
{
	oslQuit();
	return 0;
}
#endif


//extern char game_name[MAX_PATH];

/* Exit callback */
int exit_callback(int arg1, int arg2, void *common)
{
    osl_quit = 1;
	return 0;
}

/* Callback thread */
int CallbackThread(SceSize args, void *argp)
{
	int cbid;

	cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
	sceKernelRegisterExitCallback(cbid);

	sceKernelSleepThreadCB();

	return 0;
}

#ifdef KERNEL_MODE
// Copying the exception handler from SDLSMS as it gives a nice output to find the bug
void sdl_psp_exception_handler(PspDebugRegBlock *regs)
{
	pspDebugScreenInit();

	pspDebugScreenSetBackColor(0x00FF0000);
	pspDebugScreenSetTextColor(0xFFFFFFFF);
	pspDebugScreenClear();

	pspDebugScreenPrintf("I regret to inform you your psp has just crashed\n\nException Details:\n");
	pspDebugDumpException(regs);
	pspDebugScreenPrintf("\nThe offending routine may be identified with:\n\n"
		"\tpsp-addr2line -e target.elf -f -C 0x%x 0x%x 0x%x\n\n"
		"Press START to exit.",
		regs->epc, regs->badvaddr, regs->r[31]);

	while(1)		{
		oslReadKeys();
		if (osl_keys->pressed.start)
			oslQuit();
	}
}
#endif

/* Sets up the callback thread and returns its thread id */
int SetupCallbacks(void)
{
	int thid = 0;

	thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
	if(thid >= 0)
	{
		sceKernelStartThread(thid, 0, 0);
	}

	return thid;
}

/*int (*oldPowerCallback)(int, int, void*)=NULL;

int myPowerCallback(int unknown, int pwrflags,void *common){
	int cbid;
	if ((pwrflags & PSP_POWER_CB_POWER_SWITCH))		{
		if (menuConfig.file.sramAutosave)
			machine_manage_sram(SRAM_SAVE, 0);
	}
	if (oldPowerCallback)
		return oldPowerCallback(unknown, pwrflags, common);
}*/

//extern char *__psp_argv_0;
#ifdef KERNEL_MODE
int user_main(void)
#else
int main(void)
#endif
{
//	scePowerSetClockFrequency (133, 133, 66);
	oslInit(0);
	//Old behavior (compatible with 1.0)
	oslSetImageAutoSwizzle(0);

/*	oldPowerCallback = osl_powerCallback;
	osl_powerCallback = myPowerCallback;*/

	SetupCallbacks();
#ifdef KERNEL_MODE
	sceIoChdir(startPath);
//	osl_exitCallback = exitCallback;
#endif
//	strcpy(RomPath, "ms0:/PSP/GAME/SMSPlus1.2/roms/Castle Of Illusion.sms");

/*	char *p;
	p = strrchr (RomPath, '/');
	*p = '\0';*/

/*	oslInitGfx(0, 1);
	int size = 0;
	while (malloc(1000))
		size++;
	oslDebug("%ik", size);*/

	InitConfig();

	//Sauvegarde pour plus tard
	memcpy(menuConfigDefault, &menuConfig, sizeof(menuConfig));

	SmsEmulate();

	SaveMyPlacesFile();
	sceKernelExitGame();
}

#ifdef KERNEL_MODE
int main(int argc, char *argp[])
{
    // Kernel mode thread
	int i;

	// Need to copy this across otherwise the address is in kernel mode
/*	strcpy (startPath, (char*)argp[0]);
	
	//Delete the file name after the slash
	for (i=strlen(startPath) - 1; i >= 0; i--)		{
		if (startPath[i] == '\\' || startPath[i] == '/')		{
			startPath[i] = 0;
			break;
		}
	}*/

	//Get path
	sceIoGetThreadCwd(sceKernelGetThreadId(), startPath, sizeof(startPath));

	pspDebugInstallErrorHandler(sdl_psp_exception_handler);

	if (!oslInitUsbStorage())
		gbUsbAvailable = 1;

    // create user thread, tweek stack size here if necessary
    SceUID thid = sceKernelCreateThread("User Mode Thread", user_main,
            0x12, // default priority
            256 * 1024, // stack size (256KB is regular default)
            PSP_THREAD_ATTR_USER | THREAD_ATTR_VFPU, NULL);

    // start user thread, then wait for it to do everything else
    sceKernelStartThread(thid, 0, 0);
    sceKernelWaitThreadEnd(thid, NULL);

	oslDeinitUsbStorage();

    return 0;
}
#endif
