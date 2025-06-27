
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
#include "system.h"
#include "pspcommon.h"

//Defined here, also in MENUPLUS.C
#define DEBUG_MODE

int systemInit = 0;
int frameReady = 0;
unsigned char pixel_values[256];
char gblMachineType = EM_SMS;
char gblNewGame = 0;
uLong sram_crc;
unsigned int gblCpuCycles = 0;

//Does a frame
void machine_frame(int skip)		{
	if (gblMachineType == EM_SMS)
		system_frame(skip);
	else if (gblMachineType == EM_GBC)
		gb_doFrame(skip);
}


int gb_save_sram(VIRTUAL_FILE *fd, byte *buf,int size)
{
	const int sram_tbl[]={1,1,1,4,16,8};
	int sram_size = 0x2000 * sram_tbl[size];

#ifdef CHEAT_SUPPORT
	cheat_decreate_cheat_map();
#endif
	VirtualFileWrite(buf, sram_size, 1, fd);
#ifdef CHEAT_SUPPORT
	cheat_create_cheat_map();
#endif

//	sram_crc = crc;
	return 1;
}

int gb_load_sram(VIRTUAL_FILE *fd, byte *buf, int bufsize)
{
	memset(buf, 0, bufsize);
	
	int ramsize = VirtualFileRead(buf, 1, bufsize, fd);
	if(ramsize & 4)
		renderer_set_timer_state(*(int*)(buf+ramsize-4));

	return ramsize;
}

//force: 1 to always write, 0 to only if changed
int machine_manage_sram(int mode, int force)			{
    char name[MAX_PATH];
    VIRTUAL_FILE *fd;
	int ramsize = 0;

	if(menuConfig.file.filename[0] == '\0')
		return;

	pspGetStateName(name, STATE_SRAM);

    switch(mode)
    {
        case SRAM_SAVE:
			if (gblMachineType == EM_SMS)			{
				//Find out if something was written to the SRAM (which is zero initialized)
/*				int i, modified = 0;
				for (i=0;i<0x8000;i++)		{
					if (((u8*)cart.sram)[i] != 0)
						modified = 1;
				}
				if (!modified)
					break;*/

				//Determine if something has changed (using the CRC)
				uLong crc = crc32(0L, Z_NULL, 0);
				crc = crc32(crc, cart.sram, 0x8000);
				if (sram_crc == crc)
					break;

				sram_crc = crc;
			}
			else if (gblMachineType == EM_GBC)		{
				if (!(rom_get_loaded() && rom_has_battery()))
					break;

				//Determine if something has changed (using the CRC)
				const int sram_tbl[]={1,1,1,4,16,8};
				int sram_size = 0x2000 * sram_tbl[rom_get_info()->ram_size];

				if (rom_get_info()->cart_type>=0x0f && rom_get_info()->cart_type<=0x13)			{
					int tmp = renderer_get_timer_state();
					memcpy(get_sram() + sram_size, &tmp, sizeof(int));
					sram_size += sizeof(int);
				}

				uLong crc = crc32(0L, Z_NULL, 0);
				crc = crc32(crc, get_sram(), sram_size);

				if (sram_crc == crc)
					break;

				sram_crc = crc;
			}

			if (BatteryWarning("Your battery is low!\nDo you want to save the SRAM contents?\n(This might corrupt your Memory Stick if your PSP stops during this operation.)"))			{
				fd = VirtualFileOpen(name, 0, VF_GZFILE, VF_O_WRITE);
				if (fd)
				{
					if (gblMachineType == EM_SMS)
						VirtualFileWrite(cart.sram, 0x8000, 1, fd);
					else if (gblMachineType == EM_GBC)
						gb_save_sram(fd, get_sram(), rom_get_info()->ram_size);
					VirtualFileClose(fd);
				}
			}
            break;

        case SRAM_LOAD:
            fd = VirtualFileOpen(name, 0, VF_GZFILE, VF_O_READ);
			if (gblMachineType == EM_SMS)			{
				if(fd)			{
					sms.save = 1;
					VirtualFileRead(cart.sram, 0x8000, 1, fd);
					ramsize = 0x8000;
				}
				else
					/* No SRAM file, so initialize memory */
					memset(cart.sram, 0x00, 0x8000);

				sram_crc = crc32(0L, Z_NULL, 0);
				sram_crc = crc32(sram_crc, cart.sram, 0x8000);
			}
			else if (gblMachineType == EM_GBC)		{
				if (fd)
					ramsize = gb_load_sram(fd, sram_space, sizeof(sram_space));

				sram_crc = crc32(0L, Z_NULL, 0);
				sram_crc = crc32(sram_crc, sram_space, ramsize);
			}
			if (fd)
				VirtualFileClose(fd);
            break;
    }
	return ramsize;
}

void machine_poweroff()		{
//	if (gblMachineType == EM_SMS)
//		system_poweroff();

	if (menuConfig.file.sramAutosave)
	    machine_manage_sram(SRAM_SAVE, 0);
}

int MenuPlusAction(int action, void* param)			{
	switch (action)			{
		case MA_LOADROM:
			if (systemInit)
			{
				machine_poweroff();
			}

			strcpy(menuConfig.file.filename, (char*)param);

			if(load_rom(menuConfig.file.filename) == 0) 
			{
				return 0;
			}

			if(systemInit)
			{
				if (gblMachineType == EM_SMS)			{
					sms_init();
					render_init();
					system_poweron();
				}
			}

			gblNewGame = 1;
			menuPlusTerminated = 1;
			menuConfig.video.pause = 0;
			break;
	}
	return 1;
}

//moment: -1 = avant, 0 = toujours, +1 = après
void MenuOptionsConfigure(int moment)		{
	static MENUPARAMS *m = NULL;
	if (moment == -1)		{
		if (!m)
			m = malloc(sizeof(*m));
		if (!m)
			oslFatalError("MenuOptionsConfigure#1: m == NULL");
		memcpy(m, &menuConfig, sizeof(*m));
	}
	else	{
		if (!m)
			oslFatalError("MenuOptionsConfigure#2: m == NULL");
		if (moment == 0 ||
			m->video.brightnessMode != menuConfig.video.brightnessMode ||
			m->video.gamma != menuConfig.video.gamma || m->video.vibrance != menuConfig.video.vibrance ||
			m->video.renderDepth != menuConfig.video.renderDepth)			{
			int i;

			if (gblMachineType == EM_SMS)
				bitmap.depth  = menuConfig.video.renderDepth;
			else if (gblMachineType == EM_GBC)
				bitmap.depth  = 16;
			bitmap.pitch  = 256 * bitmap.depth / 8;

			for (i=0;i<256;i++)			{
				if (menuConfig.video.brightnessMode == 1)
					//Bright & Fast
					pixel_values[i] = ((i * 232) >> 8) + 24;
				else if (menuConfig.video.brightnessMode == 3)
					//Invert
					pixel_values[i] = 255 - i;
				else
					//Normal
					pixel_values[i] = i;

				//Next: apply gamma correction
				if (menuConfig.video.gamma != 100)
					//Gamma
					pixel_values[i] = ((unsigned char)(255.0f * pow((pixel_values[i] / 255.0f), 1.0f/(((float)menuConfig.video.gamma) / 100.f)))) >> 3;
				else
					//For 15-bit mode
					pixel_values[i] >>= 3;
			}

			if (gblMachineType == EM_SMS)			{
				for (i=0;i<PALETTE_SIZE;i++)
					palette_sync(i);
			}
			else if (gblMachineType == EM_GBC)		{
				gb_invalidate_all_colors();
			}
		}
		if (moment == 0 ||
			m->video.spriteLimit != menuConfig.video.spriteLimit)		{
			if (gblMachineType == EM_SMS)
				pspSetObjRenderingRoutine();
		}

		if (m->sound.perfectSynchro != menuConfig.sound.perfectSynchro)		{
			SoundTerm();
			if(menuConfig.sound.enabled)
				SoundInit();
		}

		//La colorisation a changé?
		if (moment == 0 ||
			m->gameboy.colorization != menuConfig.gameboy.colorization)			{
			if (menuConfig.gameboy.colorization)		{
				gblModeColorIt = CheckFichierPalette();
				if (gblModeColorIt)
					OuvreFichierPalette(0, "init");
				gb_lastVramCrc = 0;
				exiting_lcdc();
			}
			else
				gblModeColorIt = 0;
		}

		//La palette a changé?
		if (moment == 0 ||
			strcmp(m->gameboy.palette, menuConfig.gameboy.palette))			{
			//Eventually load a palette
			if (menuConfig.gameboy.palette[0])
				OuvreFichierConfig("palettes.ini", menuConfig.gameboy.palette);
			else		{
				memcpy(currentPalCache, defaultPalCache, sizeof(currentPalCache));
				gb_invalidate_all_colors();
			}
		}

		//Le son a changé?
		snd_stat_upd = 1;
		if (moment == 0 ||
			m->sound.enabled != menuConfig.sound.enabled ||
			m->sound.sampleRate != menuConfig.sound.sampleRate ||
			m->sound.stereo != menuConfig.sound.stereo ||
			m->video.country != menuConfig.video.country)			{

			//Son
			if (menuConfig.sound.enabled)
			{
				if (gblMachineType == EM_SMS)		{
					snd.sample_rate    = menuConfig.sound.sampleRate;
					snd.fps            = (menuConfig.video.country == 1) ? 50 : 60;
				}
				else if (gblMachineType == EM_GBC)			{
					//Only one option available for GBC
					snd.sample_rate    = 44100;
					//No country for GBC...
					snd.fps            = 60;
				}
				snd.psg_clock      = 3579545;
				snd.mixer_callback = NULL;
				sms.use_fm         = 0;
//				snd.sample_count   = (snd.sample_rate / snd.fps);
			}
			else
				snd.sample_rate    = 0;

			//Son
			if(menuConfig.sound.enabled)
				SoundInit();
			else
				SoundTerm();

			sound_change();
		}

		if (moment == 0 ||
			m->sound.turboSound != menuConfig.sound.turboSound)		{
			//Take in account modifications of turboSound
//			if (gblMachineType == EM_SMS)			{
				if (menuConfig.video.turbo)
					SetTurbo(1);
//			}
		}

		if (moment == 0 || m->misc.pspClock != menuConfig.misc.pspClock)		{
			if (menuConfig.misc.pspClock >= 100 && menuConfig.misc.pspClock <= 333)
				scePowerSetClockFrequency(menuConfig.misc.pspClock, menuConfig.misc.pspClock, menuConfig.misc.pspClock / 2);
		}
		free(m);
		m = NULL;
	}

	gblCpuCycles = (menuConfig.misc.z80Clock * 256) / 100;
}

void machine_reset()		{
	if (gblMachineType == EM_SMS)
		system_reset(0);
	else if (gblMachineType == EM_GBC)
		gb_reset();
	//Disable turbo and pause upon start
//	SetTurbo(0);
	menuConfig.video.pause = 0;
}

static void SmsInit(void)
{

//	sceDisplayWaitVblankStart();
//	sceGuSwapBuffers();

	systemInit = 1;
}

static void SmsTerm(void)
{
	if(systemInit)
	{
		machine_poweroff();

		// Close any network resources, the check is done in the network modules
//		adhocTerm();

		SoundTerm();
		SoundWaitTerm();

		ControlsTerm();
		VideoGuTerm();

		if (gblMachineType == EM_SMS)			{
			system_shutdown();
		}
	}

	systemInit = 0;
}

void gb_doFrame(int skip)		{
	gb_run_frame(skip);
}


extern volatile int osl_vblCount, osl_vblankCounterActive, osl_nbSkippedFrames;
int gblFramerate = 0, gblVirtualFramerate = 0;

void SmsEmulate()
{
	u32 nFrame, nDrawnFrames, skip=0, framerate;
	int nbRenderedFramesPerSec, nbVirtualFramesPerSec, lastVCount, vsyncAdd;
//	int cpuTime = 0, frameskipLower = 0, skippedFrames = 0, tmp;
	int frameskip = 0;

	memset(&bitmap, 0, sizeof(bitmap_t));
	bitmap.width  = 256;
	bitmap.height = 192;
	bitmap.depth  = menuConfig.video.renderDepth;
	bitmap.pitch  = 256 * menuConfig.video.renderDepth / 8;

	ControlsInit();
	VideoGuInit();

	menuConfig.file.filename[0] = '\0';

	sms.save = 1;

	// Display the menu to load up the ROM
//	displayMenu();
	menuIsInGame = 0;

	menuPlusShowMenu();
//	MenuPlusAction(MA_LOADROM, "ms0:/PSP/GAME/SMSPlus1.2/roms/Sonic Blast.sms");

	//Bidouille pour détecter les choses qui ont changé
	MenuOptionsConfigure(-1);
	MenuOptionsConfigure(0);

	VideoGuScreenClear();

	//Système
	systemInit = 1;
	nFrame = nDrawnFrames = 0;
	menuConfig.video.turbo = menuConfig.video.pause = 0;
	nbRenderedFramesPerSec = 0;
	gblFramerate = gblVirtualFramerate = 0;
	menuUpdateRender = 0;

	//Système
	if (gblMachineType == EM_SMS)
		system_poweron();

	while(1)
	{
/*		int autoFskip = (menuConfig.video.vsync == 2);
		int targetFrameskip;
		//Auto frameskip support
		if (autoFskip)		{
			//Calculate the target frameskip (0 - 100%: 0, 100 - 200: 1, etc.)
			if (targetFrameskip > frameskip)
				frameskip = targetFrameskip;
			else if (targetFrameskip < frameskip)	{
				//We don't want to oscillate too much, so we set a certain treshold to reach before the frameskip can lower.
				frameskipLower++;
				if (frameskipLower > 20)
					frameskip--;
			}
		}
		else
			frameskip = 0;*/

		//PAL support
		if (gblMachineType == EM_SMS && menuConfig.video.country == 1)
			framerate = (menuConfig.video.slowMode ? 25 : 50);
		else
			framerate = (menuConfig.video.slowMode ? 30 : 60);
		oslSetFramerate(framerate);

		if (menuUpdateRender)
			nDrawnFrames = 0, menuUpdateRender = 0;

		if (gblMachineType == EM_SMS)
			ControlsUpdate();
		else if (gblMachineType == EM_GBC)		{
			gbe_refresh();
			gbe_updatePad();
		}

		if (osl_quit)
			break;

		//Framerate
		if (menuConfig.video.cpuTime == 2)			{
			int vcount = osl_vblCount;
			if (!skip)
				nbRenderedFramesPerSec++;
			nbVirtualFramesPerSec++;
			if (vcount / framerate > lastVCount / framerate)			{
				gblFramerate = nbRenderedFramesPerSec;
				gblVirtualFramerate = nbVirtualFramesPerSec;
				nbRenderedFramesPerSec = 0;
				nbVirtualFramesPerSec = 0;
			}
			lastVCount = vcount;
		}

		if (!menuConfig.video.pause)			{
			if (gblMachineType == EM_SMS)			{
				system_frame(skip);
				if (menuConfig.sound.turboSound /*== 2*/ || !menuConfig.video.turbo)		{
					//On peut commencer à jouer le son: tout est initialisé (sinon il faut attendre)
					soundPause = 0;
					if (!menuConfig.sound.perfectSynchro)			{
						SoundUpdate();
					}
				}
			}
			else if (gblMachineType == EM_GBC)		{
				//Run one GB frame
				gb_doFrame(skip);
				soundPause = 0;
				if (menuConfig.sound.turboSound /*== 2*/ || !menuConfig.video.turbo)		{
					soundPause = 0;
					if (!menuConfig.sound.perfectSynchro)			{
						snd_render_orig(snd.output, snd.sample_count);
						SoundUpdate();
					}
				}
			}
		}

		if (!skip && menuConfig.video.syncMode == 0)			{
//			if (gblMachineType == EM_SMS)
				VideoGuUpdate(nDrawnFrames, menuConfig.video.render);
			frameReady = 1;
			nDrawnFrames++;
		}

//		vsyncAdd = 0;

//redo:
		if (menuConfig.video.turbo)			{
			if (!skip)		{
				oslEndDrawing();
				oslSwapBuffers();
			}
			skip = nFrame % (menuConfig.video.turboSkip + 1);
		}
		else	{
			int vsync = 4 /*+ vsyncAdd*/;
			if (menuConfig.video.vsync == 2)
				vsync |= 1;
/*			else if (menuConfig.video.vsync == 1)
				vsync |= 8;*/

/*			if (autoFskip && !skip)
				tmp = oslMeanBenchmarkTestEx(OSL_BENCH_END, 0);*/

			skip = oslSyncFrameEx(oslMinMax(frameskip+1, menuConfig.video.frameskip, menuConfig.video.frameskipMax), menuConfig.video.frameskipMax, vsync);

/*			if (autoFskip && !skip)		{
				//Store the CPU usage (in percent)
				cpuTime = tmp * 6 / 1000;
				oslBenchmarkTest(OSL_BENCH_START);
			}*/
		}

/*		if (autoFskip)	{
			if (!skip)			{
				cpuTime = oslMeanBenchmarkTestEx(OSL_BENCH_GET_LAST, 0) * 6 / 1000;
				oslMeanBenchmarkTestEx(OSL_BENCH_START, 0);
				targetFrameskip = skippedFrames;
				skippedFrames = 0;
			}
			else	{
				skippedFrames++;
			}
		}*/
		if (!skip && menuConfig.video.syncMode == 1)			{
//			if (gblMachineType == EM_SMS)
				VideoGuUpdate(nDrawnFrames, menuConfig.video.render);
			frameReady = 1;
			nDrawnFrames++;
		}

		nFrame++;
		//Pal
/*		if (menuConfig.video.country == 1 && nFrame % 6 == 0)		{
			if (!skip)
				vsyncAdd = 16;
			goto redo;
		}*/
		//In game, menu not currently shown - WARNING: DONT MOVE THIS, THE MESSAGEBOX CODE RELIES ON menuIsInGame == 2 TO KNOW IF THE GAME IS CURRENTLY RUNNING (or menuIsInGame == 1 = menu is shown)
		menuIsInGame = 2;

		if (menuDisplaySpecialMessage)
			MenuSpecialMessageDisplay();
	}

	SmsTerm();
}

/* Save or load SRAM */
#if 0
void system_manage_sram(uint8 *sram, int slot, int mode)
{
    char name[MAX_PATH];
	char *ptr;
    FILE *fd;

	if(menuConfig.file.wifiEnabled)
	{
		memset(sram, 0x00, 0x8000);
		return;
	}

	if(menuConfig.file.filename[0] == '\0')
		return;

    strcpy(name, menuConfig.file.filename);
    strcpy(strrchr(name, '.'), ".sram");

    switch(mode)
    {
        case SRAM_SAVE:
//            if(sms.save)
//            {
				if (BatteryWarning("Your battery is low!\nDo you want to save the SRAM contents?\n(This might corrupt your Memory Stick if your PSP stops during this operation.)"))			{
					fd = fopen(name, "wb");
					if(fd)
					{
						fwrite(sram, 0x8000, 1, fd);
						fclose(fd);
					}
				}
//            }
            break;

        case SRAM_LOAD:
            fd = fopen(name, "rb");
            if(fd)
            {
                sms.save = 1;
                fread(sram, 0x8000, 1, fd);
                fclose(fd);
            }
            else
            {
                /* No SRAM file, so initialize memory */
                memset(sram, 0x00, 0x8000);
            }
            break;
    }
}
#endif
