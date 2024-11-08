
#include <pspctrl.h>
#include "shared.h"
#include "PspSound.h"
#include "system.h"
#include "pspcommon.h"

#define UPPER_THRESHOLD 0xcf
#define LOWER_THRESHOLD 0x2f

int count = 0;

void ControlsInit(void)
{
	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
}

void ControlsTerm(void)
{
	// Nothing to be done here
}

void ControlsShowMenu()		{
	SoundPause();
	gblNewGame = 0;
	MenuOptionsConfigure(-1);
	menuPlusShowMenu();
	if (gblNewGame)
		//Reconfigure all if a new game was selected (especially if it's not the same machine...)
		MenuOptionsConfigure(0);
	else
		MenuOptionsConfigure(1);
//	if (menuConfig.video.background == 1)
//		VideoGuScreenClear();
	//Clearer les premières frames
	menuUpdateRender = 1;
	SoundResume();
}

void MiscUpdate()		{
	menuHandleStatusMessage();
}

void ControlsUpdate(void)
{
	int err = 0;
	int dataRcvd = 0;
	static u32 rateCounter = 0;

	SceCtrlData pspPad;
	SceCtrlData rcvPad;

	SceCtrlData *pad1 = &pspPad;
	SceCtrlData *pad2 = 0;


	// Used to limit the amount of data sent if using adhoc to play
	if(menuConfig.file.wifiEnabled)
	{
		count++;
	
		if(count < 5)
			return;
	}
	count = 0;

	sceCtrlPeekBufferPositive(pad1, 1);
	menuKeysAnalogApply((u32*)&pad1->Buttons, (signed)pad1->Lx-128, (signed)pad1->Ly-128);
	//SUPPRIMER
//	MyReadKeys();

	pad1->Buttons = ControlsMenuUpdate(pad1->Buttons);

/*	if(menuConfig.file.wifiEnabled)
	{
		if(menuConfig.file.wifiServer)
		{
			unsigned int length = sizeof(rcvPad);
//			adhocSend(&pspPad, length);
//			adhocRecvBlocked(&rcvPad, &length);

			pad1 = &pspPad;
			pad2 = &rcvPad;
		}
		else
		{
			unsigned int length = sizeof(rcvPad);
//			adhocRecvBlocked(&rcvPad, &length);
//			adhocSend(&pspPad, sizeof(pspPad));

			pad1 = &rcvPad;
			pad2 = &pspPad;
		}
	}*/

	input.pad[0] = 0;
	input.pad[1] = 0;
	input.system = 0;

	if(pad1->Buttons != 0)
	{
		if(pad1->Buttons & menuConfig.ctrl.keys.up) input.pad[0] |= INPUT_UP;
		if(pad1->Buttons & menuConfig.ctrl.keys.down) input.pad[0] |= INPUT_DOWN;
		if(pad1->Buttons & menuConfig.ctrl.keys.left) input.pad[0] |= INPUT_LEFT;
		if(pad1->Buttons & menuConfig.ctrl.keys.right) input.pad[0] |= INPUT_RIGHT;
		if(pad1->Buttons & menuConfig.ctrl.keys.button1) input.pad[0] |= INPUT_BUTTON1;
		if(pad1->Buttons & menuConfig.ctrl.keys.button2) input.pad[0] |= INPUT_BUTTON2;
		if(pad1->Buttons & menuConfig.ctrl.keys.auto1)		{
			if (rateCounter % menuConfig.ctrl.autofireRate == 0)
				input.pad[0] |= INPUT_BUTTON1;
		}
		if(pad1->Buttons & menuConfig.ctrl.keys.auto2)		{
			if (rateCounter % menuConfig.ctrl.autofireRate == 0)
				input.pad[0] |= INPUT_BUTTON2;
		}

		if(pad1->Buttons & menuConfig.ctrl.keys.start)
			input.system |= (IS_GG) ? INPUT_START : INPUT_PAUSE;		
	}

/*	if(pad2)
	{
		// Check analogue stick
		if (pad2->Ly > UPPER_THRESHOLD) input.pad[1] |= INPUT_DOWN;
		if (pad2->Ly < LOWER_THRESHOLD) input.pad[1] |= INPUT_UP;
		if (pad2->Lx < LOWER_THRESHOLD) input.pad[1] |= INPUT_LEFT;
		if (pad2->Lx > UPPER_THRESHOLD) input.pad[1] |= INPUT_RIGHT;

		if(pad2->Buttons != 0)
		{
			if(pad2->Buttons & PSP_CTRL_UP) input.pad[1] |= INPUT_UP;
			if(pad2->Buttons & PSP_CTRL_DOWN) input.pad[1] |= INPUT_DOWN;
			if(pad2->Buttons & PSP_CTRL_LEFT) input.pad[1] |= INPUT_LEFT;
			if(pad2->Buttons & PSP_CTRL_RIGHT) input.pad[1] |= INPUT_RIGHT;
			if(pad2->Buttons & PSP_CTRL_CROSS) input.pad[1] |= (IS_GG) ? INPUT_BUTTON2 : INPUT_BUTTON1;
			if(pad2->Buttons & PSP_CTRL_CIRCLE) input.pad[1] |= (IS_GG) ? INPUT_BUTTON1 : INPUT_BUTTON2;

			if(pad2->Buttons & PSP_CTRL_START)
				input.system |= (IS_GG) ? INPUT_START : INPUT_PAUSE;		

			if(pad2->Buttons & PSP_CTRL_LTRIGGER)
			{
				ControlsShowMenu();
			}
		}
	}*/

	MiscUpdate();
	rateCounter++;
}

//L'ancienne
/*void ControlsUpdate(void)
{
	int err = 0;
	int dataRcvd = 0;
	SceCtrlData pspPad;
	SceCtrlData rcvPad;

	SceCtrlData *pad1 = &pspPad;
	SceCtrlData *pad2 = 0;


	// Used to limit the amount of data sent if using adhoc to play
	if(SmsConfig.WifiEnabled)
	{
		count++;
	
		if(count < 5)
			return;
	}
	count = 0;

	sceCtrlPeekBufferPositive(&pspPad, 1);
	
	if(SmsConfig.WifiEnabled)
	{
		if(SmsConfig.Server)
		{
			unsigned int length = sizeof(rcvPad);
//			adhocSend(&pspPad, length);
//			adhocRecvBlocked(&rcvPad, &length);

			pad1 = &pspPad;
			pad2 = &rcvPad;
		}
		else
		{
			unsigned int length = sizeof(rcvPad);
//			adhocRecvBlocked(&rcvPad, &length);
//			adhocSend(&pspPad, sizeof(pspPad));

			pad1 = &rcvPad;
			pad2 = &pspPad;
		}
	}

	input.pad[0] = 0;
	input.pad[1] = 0;
	input.system = 0;

	// Check analogue stick
	if (pad1->Ly > UPPER_THRESHOLD) input.pad[0] |= INPUT_DOWN;
	if (pad1->Ly < LOWER_THRESHOLD) input.pad[0] |= INPUT_UP;
	if (pad1->Lx < LOWER_THRESHOLD) input.pad[0] |= INPUT_LEFT;
	if (pad1->Lx > UPPER_THRESHOLD) input.pad[0] |= INPUT_RIGHT;

	if(pad1->Buttons != 0)
	{
		if(pad1->Buttons & PSP_CTRL_UP) input.pad[0] |= INPUT_UP;
		if(pad1->Buttons & PSP_CTRL_DOWN) input.pad[0] |= INPUT_DOWN;
		if(pad1->Buttons & PSP_CTRL_LEFT) input.pad[0] |= INPUT_LEFT;
		if(pad1->Buttons & PSP_CTRL_RIGHT) input.pad[0] |= INPUT_RIGHT;
		if(pad1->Buttons & PSP_CTRL_CROSS) input.pad[0] |= INPUT_BUTTON1;
		if(pad1->Buttons & PSP_CTRL_CIRCLE) input.pad[0] |= INPUT_BUTTON2;

		if(pad1->Buttons & PSP_CTRL_START)
			input.system |= (IS_GG) ? INPUT_START : INPUT_PAUSE;		

		if(pad1->Buttons & PSP_CTRL_SELECT)
		{
			SoundPause();
			displayMenu();
			SoundResume();
		}
	}

	if(pad2)
	{
		// Check analogue stick
		if (pad2->Ly > UPPER_THRESHOLD) input.pad[1] |= INPUT_DOWN;
		if (pad2->Ly < LOWER_THRESHOLD) input.pad[1] |= INPUT_UP;
		if (pad2->Lx < LOWER_THRESHOLD) input.pad[1] |= INPUT_LEFT;
		if (pad2->Lx > UPPER_THRESHOLD) input.pad[1] |= INPUT_RIGHT;

		if(pad2->Buttons != 0)
		{
			if(pad2->Buttons & PSP_CTRL_UP) input.pad[1] |= INPUT_UP;
			if(pad2->Buttons & PSP_CTRL_DOWN) input.pad[1] |= INPUT_DOWN;
			if(pad2->Buttons & PSP_CTRL_LEFT) input.pad[1] |= INPUT_LEFT;
			if(pad2->Buttons & PSP_CTRL_RIGHT) input.pad[1] |= INPUT_RIGHT;
			if(pad2->Buttons & PSP_CTRL_CROSS) input.pad[1] |= INPUT_BUTTON1;
			if(pad2->Buttons & PSP_CTRL_CIRCLE) input.pad[1] |= INPUT_BUTTON2;

			if(pad2->Buttons & PSP_CTRL_START)
				input.system |= (IS_GG) ? INPUT_START : INPUT_PAUSE;		

			if(pad2->Buttons & PSP_CTRL_SELECT)
			{
				SoundPause();
				displayMenu();
				SoundResume();
			}
		}
	}
}*/
