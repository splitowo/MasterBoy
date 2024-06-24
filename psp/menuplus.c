#include "pspcommon.h"
#include "specialtext.h"
#include "sound.h"
#include "menuplusint.h"

//Defined here, also in SMS.C
#define DEBUG_MODE

/*
	Stick analogique: bits 24-27 de held.value
	Chercher PADDING et enlever!!!
	(c'est là juste pour fixer les pertes de performances dans certaines build)
	Référence: 82% sonic 1 jungle zone
*/

#ifdef DEBUG_MODE
	int menuDebugRamShow = 0;
#endif

/*
	DEFINES
*/

int LARGEUR_MENU = 200;
int MENU_MAX_DISPLAYED_OPTIONS=5;
screen_t Screen __attribute__((aligned(64)));

/*
	Variables globales
*/
OSL_IMAGE *imgIcons = NULL, *imgNumbers = NULL, *imgBord = NULL, *imgBack = NULL;
OSL_FONT *ftStandard = NULL, *ftGlow = NULL;
HINT gblHint;
CHOICEMENU gblChoice;
MENUPARAMS menuConfig;
WINDOW *menuCurrentWindow = NULL;
WINDOW winMsgBox;
static u32 imgBord_data[64] =		{
	0x00011000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00022100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00022210, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00022221, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00022221, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
};
int fadeLevel, fadeDirection, fadeReason, menuAnimateCursor;
SceIoDirent dirEntry;
char menuFileSelectPath[MAX_PATH];
char menuFileSelectFileName[MAX_PATH];
int menuPlusTerminated, menuMainSelectedOption = 0;
MENUPRESSEDKEYS menuPressedKeys;
int menuPressedKeysAutorepeatInit, menuPressedKeysAutorepeatDelay, menuPressedKeysAutorepeatMask;
char menuStatusMessage[200];
int menuStatusActive;
int menuStatusPosition = 0, menuStatusDrawn = 0;
int menuStickMessageDisplayed = 0;
int gblMenuAlpha, menuFrameNb;
int menuTimeZone, menuDayLightSaving, menuIsInGame, menuUpdateRender;
char *menuStatusBarMessage = NULL;
char menuStatusBarMessageInd[256];
int gblFlagLoadParams;
//4 => 4000: PADDING!
u32 menuColorCursor[4], menuColorSubmenuTitle;
u32 gblShortcutKey = -1;
char menuTempMessageText[256];
int menuMusicLocked;
int gblConfigAutoShowCrc = 0, gblModeColorIt = 0;
MENUPARAMS *menuConfigDefault, *menuConfigUserDefault, *menuConfigUserMachineDefault;
char gblColorItPaletteFileName[MAX_PATH];
int menuDisplaySpecialMessage = 0;
static char tmpSlotTextMem[10][32];


/*
	Fonctions spéciales
*/

//Strcpy mais avec une taille max ^^
void safe_strcpy(char *dst, char *src, u32 maxlen)		{
	while (--maxlen && *src)
		*dst++ = *src++;
	*dst++ = 0;
}

//Strcat mais avec une taille max
void safe_strcat(char *dst, char *src, u32 maxlen)		{
	while (maxlen && *dst)
		dst++, maxlen--;
	while (--maxlen && *src)
		*dst++ = *src++;
	*dst++ = 0;
}

//Dessin d'image avec dimensions en float
void menuDrawImageSmallFloat(OSL_IMAGE *img, float x, float y, float stretchX, float stretchY)				{
		OSL_PRECISE_VERTEX *vertices;

		// do a striped blit (takes the page-cache into account)
		oslSetTexture(img);

		vertices = (OSL_PRECISE_VERTEX*)sceGuGetMemory(2 * sizeof(OSL_PRECISE_VERTEX));

		vertices[0].u = img->offsetX0;
		vertices[0].v = img->offsetY0;
		vertices[0].x = x;
		vertices[0].y = y;
		vertices[0].z = 0;

		vertices[1].u = img->offsetX1;
		vertices[1].v = img->offsetY1;
		vertices[1].x = x + stretchX;
		vertices[1].y = y + stretchY;
		vertices[1].z = 0;

		sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_2D,2,0,vertices);
}

int GetTextBoxHeight(int x0, int x1, const char *text)
{
	int x,y, x2;
	unsigned char c, newLine = 1;
	const char *text2;
	x = x0;
	y = 0;
	while(*text)		{
		c = *text;
		if (c == ' ')
		{
			text2 = text;
			x2 = x;
			do		{
				x2 += osl_curFont->charWidths[(int)(*text2++)];
				if (x2 > x1)		{
					text++;
					goto newline;
				}
			} while(*text2 != '\n' && *text2 != ' ' && *text2);
		}
		if (x + osl_curFont->charWidths[c] > x1 || *text=='\n')			{
newline:
			if (newLine && *text=='\n')
				y += osl_curFont->charHeight;
			//Prochaine ligne
			x = x0;
			newLine = 1;
			//Retour -> saute
			if (*text == '\n')
				text++;
			continue;
		}
		if (newLine)
			y += osl_curFont->charHeight;
		newLine = 0;
		x += osl_curFont->charWidths[c];
		text++;
	}
	return y;
}

typedef struct		{
	int x0, y0, x1, y1;
} RECT;

#define myResetScreenClipping()			oslSetScreenClipping(0, 0, osl_curBuf->sizeX, osl_curBuf->sizeY)
static RECT menuCurrentClipping;

void mySetScreenClipping(int x0, int y0, int x1, int y1)		{
	menuCurrentClipping.x0 = x0;
	menuCurrentClipping.y0 = y0;
	menuCurrentClipping.x1 = x1;
	menuCurrentClipping.y1 = y1;
	oslSetScreenClipping(x0, y0, x1, y1);
}

void mySetScreenClippingEx(RECT *clip)		{
	mySetScreenClipping(clip->x0, clip->y0, clip->x1, clip->y1);
}

void MySetTextColor(OSL_COLOR color)			{
	int i;
	if (!osl_curFont)
		return;
	oslSyncDrawing();
	//Fond toujours transparent
	((unsigned long*)osl_curFont->img->palette->data)[0] = 0;
	((unsigned long*)osl_curFont->img->palette->data)[1] = color;
	((unsigned long*)osl_curFont->img->palette->data)[2] = RGB(32, 32, 32);
	int m = 0x100;
	for (i=3;i<7;i++)			{
//		int m = 0x130 - i*0x10;
		((unsigned long*)osl_curFont->img->palette->data)[i] = ((color & 0xff) * m) >> 8;
		((unsigned long*)osl_curFont->img->palette->data)[i] |= (((color & 0xff00) * m) >> 8) & 0xff00;
		((unsigned long*)osl_curFont->img->palette->data)[i] |= (((color & 0xff0000) * m) >> 8) & 0xff0000;
		((unsigned long*)osl_curFont->img->palette->data)[i] |= color & 0xff000000;
	}
	osl_curPalette = NULL;
	sceKernelDcacheWritebackInvalidateRange(osl_curFont->img->palette->data, 16*4);

	//To avoid OSLib being tinting the characters. Needed with the new text system.
	oslSetTextColor(RGB(255, 255, 255));
}

void MySetTextColorEx(OSL_COLOR color, int facteurNoircissement)			{
	int i;
	int m;
	if (!osl_curFont)
		return;
	oslSyncDrawing();
	//Fond toujours transparent
	((unsigned long*)osl_curFont->img->palette->data)[0] = 0;
	((unsigned long*)osl_curFont->img->palette->data)[1] = color;
	((unsigned long*)osl_curFont->img->palette->data)[2] = RGBA(16, 16, 16, 0) | (color & 0xff000000);
	m = 0x100;
	for (i=3;i<7;i++)			{
		m -= facteurNoircissement;
//		int m = 0x130 - i*0x10;
		((unsigned long*)osl_curFont->img->palette->data)[i] = ((color & 0xff) * m) >> 8;
		((unsigned long*)osl_curFont->img->palette->data)[i] |= (((color & 0xff00) * m) >> 8) & 0xff00;
		((unsigned long*)osl_curFont->img->palette->data)[i] |= (((color & 0xff0000) * m) >> 8) & 0xff0000;
		((unsigned long*)osl_curFont->img->palette->data)[i] |= color & 0xff000000;
	}
	osl_curPalette = NULL;
	sceKernelDcacheWritebackInvalidateRange(osl_curFont->img->palette->data, 16*4);
}

void MyDrawLine(int x0, int y0, int x1, int y1, OSL_COLOR color)		{
/*	if (osl_currentAlphaEffect == OSL_FX_ALPHA)
		color = (color & 0xffffff) | (((((color & 0xff000000) >> 24) * osl_currentAlphaCoeff) >> 8) << 24);*/
	oslDrawLine(x0, y0, x1, y1, color);
}

void MyDrawGradientRect(int x0, int y0, int x1, int y1, OSL_COLOR color1, OSL_COLOR color2, OSL_COLOR color3, OSL_COLOR color4)		{
/*	if (osl_currentAlphaEffect == OSL_FX_ALPHA)		{
		color1 = (color1 & 0xffffff) | (((((color1 & 0xff000000) >> 24) * osl_currentAlphaCoeff) >> 8) << 24);
		color2 = (color2 & 0xffffff) | (((((color2 & 0xff000000) >> 24) * osl_currentAlphaCoeff) >> 8) << 24);
		color3 = (color3 & 0xffffff) | (((((color3 & 0xff000000) >> 24) * osl_currentAlphaCoeff) >> 8) << 24);
		color4 = (color4 & 0xffffff) | (((((color4 & 0xff000000) >> 24) * osl_currentAlphaCoeff) >> 8) << 24);
	}*/
	oslDrawGradientRect(x0, y0, x1, y1, color1, color2, color3, color4);
}

void MyDrawFillRect(int x0, int y0, int x1, int y1, OSL_COLOR color)		{
/*	if (osl_currentAlphaEffect == OSL_FX_ALPHA)
		color = (color & 0xffffff) | (((((color & 0xff000000) >> 24) * osl_currentAlphaCoeff) >> 8) << 24);*/
	oslDrawFillRect(x0, y0, x1, y1, color);
}

void MyDrawRect(int x0, int y0, int x1, int y1, OSL_COLOR color)		{
/*	if (osl_currentAlphaEffect == OSL_FX_ALPHA)
		color = (color & 0xffffff) | (((((color & 0xff000000) >> 24) * osl_currentAlphaCoeff) >> 8) << 24);*/
	oslDrawRect(x0, y0, x1, y1, color);
}

void menuKeysAnalogApply(u32 *keys, int aX, int aY)		{
	int decal = menuConfig.ctrl.analog.toPad ? 0 : 20;
	if (menuConfig.ctrl.analog.useCalibration)		{
		if (aY >= menuConfig.ctrl.analog.calValues[1] + menuConfig.ctrl.analog.treshold)
			*keys |= OSL_KEYMASK_DOWN << decal;
		if (aY <= menuConfig.ctrl.analog.calValues[0] - menuConfig.ctrl.analog.treshold)
			*keys |= OSL_KEYMASK_UP << decal;
		if (aX >= menuConfig.ctrl.analog.calValues[3] + menuConfig.ctrl.analog.treshold)
			*keys |= OSL_KEYMASK_RIGHT << decal;
		if (aX <= menuConfig.ctrl.analog.calValues[2] - menuConfig.ctrl.analog.treshold)
			*keys |= OSL_KEYMASK_LEFT << decal;
	}
	else	{
		if (aY >= menuConfig.ctrl.analog.treshold)
			*keys |= OSL_KEYMASK_DOWN << decal;
		if (aY <= -menuConfig.ctrl.analog.treshold)
			*keys |= OSL_KEYMASK_UP << decal;
		if (aX >= menuConfig.ctrl.analog.treshold)
			*keys |= OSL_KEYMASK_RIGHT << decal;
		if (aX <= -menuConfig.ctrl.analog.treshold)
			*keys |= OSL_KEYMASK_LEFT << decal;
	}
}

OSL_CONTROLLER *MyReadKeys()
{
	SceCtrlData ctl;
	u32 oldButtons;
	
	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(1);
	sceCtrlPeekBufferPositive( &ctl, 1 );
	osl_keys->analogX = (signed)ctl.Lx-128;
	osl_keys->analogY = (signed)ctl.Ly-128;
	menuKeysAnalogApply((u32*)&ctl.Buttons, osl_keys->analogX, osl_keys->analogY);
	oldButtons = ctl.Buttons;

	/*
		CODE AUTO-REPEAT
	*/
	if (osl_keys->autoRepeatInterval > 0)			{					//Auto repeat activé?
		//Si ça change -> compteur à zéro
		if (osl_keys->lastHeld.value != ctl.Buttons)
			osl_keys->autoRepeatCounter=0;
		else			{
			osl_keys->autoRepeatCounter++;
			if (osl_keys->autoRepeatCounter >= osl_keys->autoRepeatInit)			{
				//AutoRepeat déclenché -> déclenchement toutes les autoRepeatInterval coups
				if ((osl_keys->autoRepeatCounter - osl_keys->autoRepeatInit) % osl_keys->autoRepeatInterval == 0)
					osl_keys->lastHeld.value &= ~osl_keys->autoRepeatMask;
			}
		}
	}

	//Trop long => désactive le stick pour le menu (il y a un problème)
	if (osl_keys->autoRepeatCounter > 1000 && (ctl.Buttons & ~oldButtons) && menuConfig.ctrl.analog.toPad
		&& osl_keys->lastHeld.value)				{
		menuConfig.ctrl.analog.toPad = 0;
		UpdateMenus(MENUUPD_CTRL);
		if (!menuStickMessageDisplayed)
			MessageBox("The stick seems to be blocked. It has been deactivated, you can re-enable it through options.", "Information", MB_OK);
		menuStickMessageDisplayed = 1;
	}

	//Do not take in account keys the first frames...
//	if (menuFrameNb >= 4)
		osl_keys->pressed.value = ~osl_keys->lastHeld.value & ctl.Buttons;
//	else
//		osl_keys->pressed.value = 0;
	osl_keys->held.value = ctl.Buttons;
	osl_keys->lastHeld.value = ctl.Buttons;

	//Make sure everything is locked!
	if (menuMusicLocked)		{
		osl_keys->held.value = 0;
		osl_keys->pressed.value = 0;
	}

	//Gérer les shortcuts
	menuGetMenuKeys((u32*)&osl_keys->held.value);

	return osl_keys;
}

int alphaSave[2];

void PushAlpha()
{
	alphaSave[0] = osl_currentAlphaCoeff;
	alphaSave[1] = osl_currentAlphaEffect;
}

void PopAlpha()
{
	osl_currentAlphaCoeff = alphaSave[0];
	osl_currentAlphaEffect = alphaSave[1];
}

int GetStringWidth(const char *str)
{
	int r = 0;
	while(*str)		{
		r += osl_curFont->charWidths[(unsigned char)*str++];
	}
	return r;
}

int mod(int x, int y)		{
	x = x % y;
	if (x < 0)
		x += y;
	return x;
}

float mod_f(float x, float y)		{
	if (x < 0)
		x += y * (int)((-x)/y);
	if (x >= y)
		x -= y * (int)(x/y);
	if (x < 0)
		x += y;
	return x;
}

float rangeAnglef(float angle)
{
	if (angle >= 360.f)
		angle -= 360.f;
	if (angle < 0.f)
		angle += 360.f;
	return angle;
}

//Adapte angle2 à angle1
float adapteAnglef(float angle1, float angle2)
{
	if (angle2 > angle1 + 180.f)
		angle2 -= 360.f;
	if (angle2 < angle1 - 180.f)
		angle2 += 360.f;
	return angle2;
}

void menuGetMenuKeys(u32 *pad)		{
	int key, keyFound = -1, keyFoundValue = 0;

	//Parcourt toutes les touches
	for (key=0;key<MENUKEY_LAST_ONE;key++)		{
		int thisKey = 0;
		if (menuConfig.ctrl.acuts[key])			{
			if (((*pad) & menuConfig.ctrl.acuts[key]) == menuConfig.ctrl.acuts[key])		{
				if (!menuPressedKeys.acuts[key])			{
					menuPressedKeys.acuts[key] = 1;
					thisKey = 1;
				}
				//Autorepeat pour cette touche?
				else if (menuPressedKeysAutorepeatMask & (1 << key))		{
					menuPressedKeys.acuts[key]++;
					if (menuPressedKeys.acuts[key] >= menuPressedKeysAutorepeatInit)			{
						//AutoRepeat déclenché
						if ((menuPressedKeys.acuts[key] - menuPressedKeysAutorepeatInit) % menuPressedKeysAutorepeatDelay == 0)			{
							//Déclenchement une nouvelle fois
							menuPressedKeys.acuts[key] -= menuPressedKeysAutorepeatDelay;
							thisKey = 1;
						}
					}
				}
			}
			else
				menuPressedKeys.acuts[key] = 0;
		}
		
		if (thisKey)		{
			//Donne une valeur plus grande aux raccourcis dont le plus grand nombre de touches correspond
			int j, k = 0;
			for (j=0;j<32;j++)		{
				if ((menuConfig.ctrl.acuts[key] & (*pad)) & (1 << j))
					k++;
			}
			if (k > keyFoundValue)			{
				keyFound = key;
				keyFoundValue = k;
			}
		}
	}

	//Ca merde avec le R + haut / bas dans les listes!
/*	if (keyFound >= 0)			{
		//Ne pas être prise en compte par les autres
		*pad &= ~menuConfig.ctrl.acuts[keyFound];
		if (pressed)
			*pressed &= ~menuConfig.ctrl.acuts[keyFound];
	}*/

	gblShortcutKey = keyFound;
}

int menuGetMenuKey(u32 *pad, int key)		{
	if (menuPressedKeys.acuts[key])
		//Pour simuler comment ça se passait avant
		*pad &= ~menuConfig.ctrl.acuts[key];

	if (gblShortcutKey == key)
		return 1;
	else
		return 0;

/*	if (!menuConfig.ctrl.acuts[key])
		return 0;
	if (((*pad) & menuConfig.ctrl.acuts[key]) == menuConfig.ctrl.acuts[key])		{
		//Ne pas être prise en compte par les autres
		*pad &= ~menuConfig.ctrl.acuts[key];
		if (!menuPressedKeys.acuts[key])			{
			menuPressedKeys.acuts[key] = 1;
			return 1;
		}
		//Autorepeat pour cette touche?
		else if (menuPressedKeysAutorepeatMask & (1 << key))		{
			menuPressedKeys.acuts[key]++;
			if (menuPressedKeys.acuts[key] >= menuPressedKeysAutorepeatInit)			{
				//AutoRepeat déclenché
				if ((menuPressedKeys.acuts[key] - menuPressedKeysAutorepeatInit) % menuPressedKeysAutorepeatDelay == 0)			{
					//Déclenchement une nouvelle fois
					menuPressedKeys.acuts[key] -= menuPressedKeysAutorepeatDelay;
					return 1;
				}
			}
		}
	}
	else
		menuPressedKeys.acuts[key] = 0;
	return 0;*/
}


/*
	Menus
*/

//Loads an image in 32 bits mode and converts it to 16-bit using the GPU and its dithering.
OSL_IMAGE *loadBackgroundImage(char *filename)		{
	OSL_IMAGE *img = NULL, *temp;
	temp = oslLoadImageFile(filename, OSL_IN_RAM, OSL_PF_8888);
	if (temp)			{
		if (temp->sizeX <= 480 && temp->sizeY <= 272)			{
			img = oslCreateImage(temp->sizeX, temp->sizeY, OSL_IN_VRAM, OSL_PF_5650);
			if (img)		{
				oslStartDrawing();
				oslSetDrawBuffer(img);
				oslSetDithering(1);
				oslSetBilinearFilter(0);
				oslDrawImage(temp);
				oslSetDrawBuffer(OSL_DEFAULT_BUFFER);
				oslEndDrawing();
				oslSwizzleImage(img);
			}
		}
		oslDeleteImage(temp);
	}
	return img;
}

char *FctImgBack(int type, char *ptr)		{
	OSL_IMAGE *imgSav = imgBack;
	char *nom;
	nom = GetChaine(&ptr);
	if (nom)		{
		imgBack = loadBackgroundImage(nom);
		if (!imgBack)
			imgBack = imgSav;
		else if (imgSav)
			oslDeleteImage(imgSav);
	}
	return NULL;
}

char *FctImgIcons(int type, char *ptr)		{
	OSL_IMAGE *imgSav = imgIcons;
	char *nom;
	nom = GetChaine(&ptr);
	if (nom)		{
		oslSetTransparentColor(RGB(255, 0, 254));
		imgIcons = oslLoadImageFile(nom, OSL_IN_RAM, OSL_PF_5551);
		oslDisableTransparentColor();
		if (!imgIcons)
			imgIcons = imgSav;
		else if (imgSav)
			oslDeleteImage(imgSav);
	}
	return NULL;
}

char *FctImgNumbers(int type, char *ptr)		{
	OSL_IMAGE *imgSav = imgNumbers;
	char *nom;
	nom = GetChaine(&ptr);
	if (nom)		{
		oslSetTransparentColor(RGB(255, 0, 254));
		imgNumbers = oslLoadImageFile(nom, OSL_IN_RAM, OSL_PF_5551);
		oslDisableTransparentColor();
		if (!imgNumbers)
			imgNumbers = imgSav;
		else if (imgSav)
			oslDeleteImage(imgSav);
	}
	return NULL;
}

char *FctFtStandard(int type, char *ptr)		{
	OSL_FONT *ftSav = ftStandard;
	char *nom;
	nom = GetChaine(&ptr);
	if (nom)		{
		ftStandard = oslLoadFontFile(nom);
		if (!ftStandard)
			ftStandard = ftSav;
		else if (ftSav)
			oslDeleteFont(ftSav);
	}
	return NULL;
}

char *FctFtGlow(int type, char *ptr)		{
	OSL_FONT *ftSav = ftGlow;
	char *nom;
	nom = GetChaine(&ptr);
	if (nom)		{
		ftGlow = oslLoadFontFile(nom);
		if (!ftGlow)
			ftGlow = ftSav;
		else if (ftSav)
			oslDeleteFont(ftSav);
	}
	return NULL;
}


void menuDisplayStatusMessage(char *message, int time)			{
	safe_strcpy(menuStatusMessage, message, sizeof(menuStatusMessage));
	//Par défaut: 2 secondes
	if (time == -1)
		time = 120;
	menuStatusActive = time;
	//S'assure qu'on est positif
	menuStatusPosition = max(menuStatusPosition, 0);
}

void menuHandleStatusMessage()		{
	if (menuStatusActive)		{
		menuStatusActive--;
		menuStatusPosition = min(menuStatusPosition + 2, 13);
	}
	else
		menuStatusPosition = max(menuStatusPosition - 2, -4);
}

void menuDrawStatusMessage()		{
	if (menuStatusPosition > 0)		{
		menuStatusDrawn = 2;
		oslDrawString(0, 272 - menuStatusPosition, menuStatusMessage);
	}
	else
		menuStatusDrawn = max(menuStatusDrawn - 1, 0);
}


int GestionEventStandardInt(SUBMENU *menu, SUBMENUITEM *sub, u32 event, u32 typeUpd, int *value)
{
	if (event == EVENT_SET)		{
		if (menu->type == 3)			{
			char str[100];
			//%03i
			sprintf(str, "%%0%ii", ((SUBMENUMINMAX*)menu->items)->width);
			sprintf(((SUBMENUMINMAX*)menu->items)->value, str, *value);
		}
		else		{
			//Pas trouvé => option par défaut
			if (!SelectSubMenuItemPositionByValueInt(menu, *value))
				SelectSubMenuItemPositionByValueInt(menu, MASK_DEFAULT_MENU_VALUE);
		}
	}
	else if (event == EVENT_SELECT)			{
		if (menu->type == 3)
			*value = GetIntFromDigits((SUBMENUMINMAX*)menu->items);
		else
			*value = sub->prop1;
		UpdateMenus(typeUpd);
	}
	else if (event == EVENT_DEFAULT)			{
		if ((u8*)value >= (u8*)(&menuConfig) && (u8*)value < (u8*)(&menuConfig) + sizeof(menuConfig))			{
			u32 offset = (u8*)value - (u8*)(&menuConfig);
			int *adr = (int*)((u32)(menuConfigDefault) + offset);
			if (*value != *adr)		{
				if (MessageBox("Do you really want to restore the system default value for this option?", "Confirmation", MB_YESNO))		{
					*value = *adr;
				}
			}
		}
		UpdateMenus(typeUpd);
	}
	return 1;
}

int fctMainMiscPspClockNumeric(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_MISC, &menuConfig.misc.pspClock);
}

int fctMainMiscPspClock(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_MISC, &menuConfig.misc.pspClock);
}

int fctMainVideoCountry(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_VIDEO, &menuConfig.video.country);
}

int fctMainVideoScaling(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_VIDEO, &menuConfig.video.render);
}

int fctMainVideoSmoothing(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_VIDEO, &menuConfig.video.smoothing);
}

int fctMainVideoFrameskip(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_VIDEO, &menuConfig.video.frameskip);
}

int fctMainVideoFrameskipMax(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_VIDEO, &menuConfig.video.frameskipMax);
}

int fctMainVideoVSync(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_VIDEO, &menuConfig.video.vsync);
}

int fctMainMiscUIBackground(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_MISC, &menuConfig.video.background);
}

int fctMainVideoGamma(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_VIDEO, &menuConfig.video.gamma);
}

int fctMainVideoCpuTime(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_VIDEO, &menuConfig.video.cpuTime);
}

int fctMainSoundEnabled(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_SOUND, &menuConfig.sound.enabled);
}

int fctMainSoundRate(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_SOUND, &menuConfig.sound.sampleRate);
}

int fctMainSoundStereo(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_SOUND, &menuConfig.sound.stereo);
}

int fctMainSoundBoost(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_SOUND, &menuConfig.sound.volume);
}

int fctMainSoundTurboSound(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_SOUND, &menuConfig.sound.turboSound);
}

int fctMainSoundSyncMode(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_SOUND, &menuConfig.sound.perfectSynchro);
}


int BatteryWarning(const char *message)		{
	if (scePowerIsLowBattery())		{
		char title[30];
		sprintf(title, "Warning (batt: %i%%)", scePowerGetBatteryLifePercent());
		return MessageBox(message, title, MB_YESNO);
	}
	else
		return 1;
}

void GetJustFileName(char *dst, char *src)		{
	int i, lastPt = -1;
	for (i=0;src[i];i++)		{
		if (src[i] == '.')
			lastPt = i;
		dst[i] = src[i];
	}
	if (lastPt >= 0)
		dst[lastPt] = 0;
}

char *extractFilePath(char *file, char *dstPath, int level)		{
	int i, curLevel = 0;
	char *fileName = dstPath;

	strcpy(dstPath, file);
	//Remove the last '/' character, so that we get the path in nameTemp and the file name in ptrFileName
	for (i=strlen(dstPath) - 1; i>=0; i--)		{
		if (dstPath[i] == '/' || dstPath[i] == '\\')		{
			if (curLevel == 0)
				fileName = dstPath + i + 1;
			//Suppression de level slashs
			if (++curLevel >= level)		{
				dstPath[i] = 0;
				break;
			}
		}
	}

	return fileName;
}

void pspGetStateNameEx(char *romFile, char *name, int slot)		{
	int bAddExtension = TRUE;

	GetJustFileName(name, romFile);
/*	if (slot >= 0 && slot <= 9)		{
		int i = strlen(name);
		name[i++] = slot + '0';
		name[i] = 0;
	}
	else if (slot == STATE_EXPORT)		{
		//Import RIN states
		if (gblMachineType == EM_GBC)			{
			char origName[MAX_PATH], nameTemp[MAX_PATH], *ptrFileName;
			int i;

			//Keep the original name...
			strcpy(origName, name);

			//Here, we'll search ../SAVE/Romname.sv0.gz and then SAVE/Romname.sv0.gz
			ptrFileName = extractFilePath(origName, nameTemp, 2);
			sprintf(name, "%s/SAVE/%s.sv0.gz", nameTemp, ptrFileName);

			//If it doesn't exist, try the second possibility
			if (!fileExists(name))		{
				ptrFileName = extractFilePath(origName, nameTemp, 1);
				sprintf(name, "%s/SAVE/%s.sv0.gz", nameTemp, ptrFileName);
			}

			bAddExtension = FALSE;
		}
		//else: nothing to do
	}*/

	if (bAddExtension)		{
		if (slot == STATE_EXPORT && gblMachineType == EM_SMS)
			//On the master system, the standard state (SMSPlus / MMSPlus) is in the same folder and named filename.sav
			strcat(name, ".sav");
		else	{
			//Save in a SAVE sub-directory
			char path[MAX_PATH], *fname, *format;
			fname = extractFilePath(name, path, 1);
			if (slot == STATE_SRAM)
				format = "%s/SAVE/%s.sav.gz";
			else if (slot == STATE_CONFIG)
				format = "%s/SAVE/%s.ini";
			else if (slot == STATE_EXPORT)
				format = "%s/SAVE/%s.svs.gz";
			else
				format = "%s/SAVE/%s.sv%i.gz";
			sprintf(name, format, path, fname, slot);
		}
	}
}

void pspGetStateName(char *name, int slot)		{
	pspGetStateNameEx(menuConfig.file.filename, name, slot);
}


void machine_load_state(VIRTUAL_FILE *fd)		{
	if (gblMachineType == EM_SMS)
		system_load_state(fd);
	else if (gblMachineType == EM_GBC)		{
		gb_restore_state(fd, NULL);
	}
}

void OuvreFichierPalette(int crc, char *fonction)		{
	if (gblModeColorIt)			{
		if (fonction)
			OuvreFichierConfig(gblColorItPaletteFileName, fonction);
		else		{
			char tempCrc[100];
			sprintf(tempCrc, "[%08x]", crc);
			tilePalReinit();
			//Si la section correspondant au crc actuel n'existe pas, on utilise le profil par défaut [default].
			if (!OuvreFichierConfig(gblColorItPaletteFileName, tempCrc))
				OuvreFichierConfig(gblColorItPaletteFileName, "[default]");
		}
	}
}

int fileExists(char *fileName)		{
	SceIoStat fileStatus;
	return (sceIoGetstat(fileName, &fileStatus) >= 0);
}

int CheckFichierPalette()		{
	//Try to open ROM-specific palette file
	GetJustFileName(gblColorItPaletteFileName, menuFileSelectFileName);
	safe_strcat(gblColorItPaletteFileName, ".pal.ini", sizeof(gblColorItPaletteFileName));

	if (!fileExists(gblColorItPaletteFileName))			{
		//Search in the shared folder, using the cart name
		sprintf(gblColorItPaletteFileName, "COLORPAK/%s.pal.ini", info.cart_name);
		if (!fileExists(gblColorItPaletteFileName))		{
			//If it still doesn't exists, use the file name
			char temp[MAX_PATH], *fname;
			fname = extractFilePath(menuFileSelectFileName, temp, 1);
			GetJustFileName(temp, fname);
			sprintf(gblColorItPaletteFileName, "COLORPAK/%s.pal.ini", temp);
		}
	}
	return fileExists(gblColorItPaletteFileName);
}

int pspLoadState(int slot)		{
	VIRTUAL_FILE *fd = NULL;
	char name[MAX_PATH];

	pspGetStateName(name, slot);
	if (slot == STATE_RAM)			{
		//Pas le bon jeu
		if (strcmp(menuRamStateFileName, name))
			return 0;
		fd = VirtualFileOpen(menuRamStateMemory, RAM_STATE_SIZE, VF_MEMORY, VF_O_READ);
	}
	else if (slot == STATE_TEMP)			{
		if (menuTempStateMemory)
			fd = VirtualFileOpen(menuTempStateMemory, RAM_STATE_SIZE, VF_MEMORY, VF_O_READ);
	}
	else		{
		fd = VirtualFileOpen(name, 0, VF_GZFILE, VF_O_READ);
		//Si le state n'existe pas, essayons avec le .gz en plus...
/*		if (!fd)		{
			strcat(name, ".gz");
		}*/
	}
	if (fd)
	{
		machine_load_state(fd);
		VirtualFileClose(fd);

		//Load the palette params
		if (/*slot != STATE_TEMP &&*/ gblMachineType == EM_GBC && gblModeColorIt)
			exiting_lcdc();

		return 1;
	}
	return 0;
}

void machine_save_state(VIRTUAL_FILE *fd)		{
	if (gblMachineType == EM_SMS)
		system_save_state(fd);
	else if (gblMachineType == EM_GBC)		{
		gb_save_state(fd, NULL);
	}
}

void makeDir(char *dir)		{
	int fd;
	fd = sceIoDopen(dir);
	if (fd < 0)		{
		sceIoMkdir(dir, 0);
		sceIoDclose(fd);
	}
}

int pspSaveState(int slot)		{
	VIRTUAL_FILE *fd = NULL;
	int success = 0;
	char name[MAX_PATH];

	pspGetStateName(name, slot);
	if (slot == STATE_RAM)		{
		fd = VirtualFileOpen(menuRamStateMemory, RAM_STATE_SIZE, VF_MEMORY, VF_O_WRITE);
		if (fd)		{
			strcpy(menuRamStateFileName, name);
			machine_save_state(fd);
			VirtualFileClose(fd);
			success = 1;
		}
	}
	else if (slot == STATE_TEMP)		{
		if (menuTempStateMemory)
			fd = VirtualFileOpen(menuTempStateMemory, RAM_STATE_SIZE, VF_MEMORY, VF_O_WRITE);
		if (fd)		{
			machine_save_state(fd);
			VirtualFileClose(fd);
			success = 1;
		}
	}
	else			{
		if (!BatteryWarning("Your battery is low!\nSaving is unlikely and might corrupt your Memory Stick. Are you sure?"))
			return 0;

		//Make sure the specified path exists - done by GZFILE
//		char path[MAX_PATH];
//		extractFilePath(name, path, 1);
//		makeDir(path);

		fd = VirtualFileOpen(name, 0, VF_GZFILE, VF_O_WRITE);
		if (fd)		{
			machine_save_state(fd);
			VirtualFileClose(fd);
			success = 1;
		}
	}
	return success;
}

char DoesSlotExists(int slot)		{
	char name[MAX_PATH];
	pspGetStateName(name, slot);
	if (slot == STATE_RAM)
		return !strcmp(menuRamStateFileName, name) && name[0];
	else
		return fileExists(name);
}

void FillExistingSlots(SUBMENU *menu, int couleur)		{
	int i;
	SUBMENUITEM *s;

	for (i=0;i<=10;i++)		{
		int slotExist;
		s = GetSubMenuItemByInt(menu, i);
		if (i < 10)			{
			char name[MAX_PATH];
			SceIoStat fileStatus;
			pspGetStateName(name, i);
			slotExist = (sceIoGetstat(name, &fileStatus) >= 0);
			if (slotExist)
				sprintf(tmpSlotTextMem[i], "Slot %i (%02i/%02i/%02i %02i:%02i)", i,
					fileStatus.sce_st_mtime.day,
					fileStatus.sce_st_mtime.month,
					fileStatus.sce_st_mtime.year % 100,
					fileStatus.sce_st_mtime.hour,
					fileStatus.sce_st_mtime.minute);
			else
				sprintf(tmpSlotTextMem[i], "Slot %i", i);
		}
		else
			slotExist = DoesSlotExists(i);
		if (couleur)		{
			s->disabled = slotExist ? 0 : 2;
			s->prop2 = couleur;
		}
		else
			s->disabled = !slotExist;
	}
}

int fctMainFileStateLoad(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	GestionEventStandardInt(menu, sub, event, MENUUPD_FILE, &menuConfig.file.state);
	if (event == EVENT_INIT)		{
//		GetSubMenuItemByInt(menu, STATE_EXPORT)->name = "Import";
		FillExistingSlots(menu, 0);
	}
	else if (event == EVENT_SELECT)		{
		//Pas de jeu chargé?
		if (!DoesSlotExists(sub->prop1))
			return 0;
		pspLoadState(sub->prop1);
		menuPlusTerminated = 1;
	}
	return 1;
}

int fctMainFileStateSave(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	GestionEventStandardInt(menu, sub, event, MENUUPD_FILE, &menuConfig.file.state);
	if (event == EVENT_INIT)		{
//		GetSubMenuItemByInt(menu, STATE_EXPORT)->name = "Export";
		FillExistingSlots(menu, RGB(128, 192, 255));
	}
	if (event == EVENT_SELECT)		{
		//Pas de jeu chargé?
		if (!menuConfig.file.filename[0])
			return 0;
		if (sub->prop1 != STATE_RAM)		{
			if (MessageBox("The selected state will be written to the Memory Stick. Are you sure?", "Confirmation", MB_YESNO))		{
				pspSaveState(sub->prop1);
				menuPlusTerminated = 1;
			}
			else
				return 0;
		}
		else	{
			pspSaveState(sub->prop1);
			menuPlusTerminated = 1;
		}
	}
	return 1;
}

int fctMainFileStateDelete(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	GestionEventStandardInt(menu, sub, event, MENUUPD_FILE, &menuConfig.file.state);
	if (event == EVENT_INIT)			{
//		GetSubMenuItemByInt(menu, STATE_EXPORT)->name = "Standard";
		FillExistingSlots(menu, 0);
	}
	if (event == EVENT_SELECT)		{
		//Pas de jeu chargé?
		if (!DoesSlotExists(sub->prop1))
			return 0;
		if (MessageBox("The selected state will be permanently deleted. Are you sure?", "Confirmation", MB_YESNO))		{
			char name[MAX_PATH];
			//Effacer l'état
			if (sub->prop1 == STATE_RAM)
				menuRamStateFileName[0] = 0;
			else	{
				if (!BatteryWarning("Your battery is low!\nErasing a file is unlikely and might corrupt your Memory Stick. Are you sure?"))
					return 0;
				pspGetStateName(name, sub->prop1);
				sceIoRemove(name);
			}
		}
		else
			return 0;
	}
	return 1;
}

int safe_strcpy_offset(char *dest, u32 offset, char *src, u32 maxLen)		{
	while (offset < maxLen - 1 && *src)
		dest[offset++] = *src++;
	dest[offset] = 0;
	return offset;
}

char *menuGetKeyName(char *dest, u32 maxLen, char *separator, u32 key)		{
	int i, j, offset = 0;
	char *keynames[32]=		{
		"Select", "", "", "Start", "\x15", "\x18",
		"\x16", "\x17", "L", "R", "", "",
		"\x11", "\x12", "\x13", "\x14", "Home", "Hold",
		"", "", "", "", "", "Note",
		"\x19", "\x1c", "\x1a", "\x1b"
	};
	int keyorder[28]=		{
		1, 2, 10, 11, 18, 19, 20, 21, 22,			//Unused
		16, 17, 23,									//Home/Hold/Note
		8, 9,										//L/R
		3, 0,										//Start/Select
		12, 13, 14, 15,								//Triangle/Rond/Croix/Carré
		4, 5, 6, 7,									//Haut/Gauche/Bas/Droite
		24, 25, 26, 27,								//Analog
	};
	if (key == 0)			{
		safe_strcpy(dest, "(none)", maxLen);
	}
	for (j=0;j<28;j++)		{
		i = keyorder[j];
		if (key & (1<<i))		{
			if (keynames[i][0])		{
				//Plus entre deux
				if (offset)
					offset = safe_strcpy_offset(dest, offset, separator, maxLen);
				//Ajoute la touche
				offset = safe_strcpy_offset(dest, offset, keynames[i], maxLen);
			}
		}
	}
	return dest;
}

#define STDKEYMASK		0xf80f3f9

int menuRedefineGetNewKey(SUBMENUITEM *sub)		{
	int key, lastValue = 0, counter;
	sprintf(menuTempMessageText, "Please press the new key for \'%s\'. Current is '%s'.", sub->name, sub->value);
	MessageBoxEx(menuTempMessageText, "Key redefinition", MB_NONE, 220);
	//Attend une ou plusieurs touche
	do			{
		MyReadKeys();
		oslWaitVSync();
	} while(osl_keys->held.value & STDKEYMASK);

	counter = 0;
	do			{
		MyReadKeys();
		oslWaitVSync();
		counter++;
	} while(!(osl_keys->held.value & STDKEYMASK) && counter < 180);

	//Attend sur une touche mais au maximum 3 secondes!
	if (counter >= 180)
		goto end;

	counter = 0;
	lastValue = (osl_keys->held.value & STDKEYMASK);
	do			{
		//S'il appuie une touche et la lâche, ça la prend en compte, mais pas les flèches
		if (((osl_keys->held.value & STDKEYMASK) == lastValue || (osl_keys->held.value & lastValue) == osl_keys->held.value) &&
			(lastValue & 0x00f000f0) == (osl_keys->held.value & 0x00f000f0) /*|| !(osl_keys->held.value & STDKEYMASK)*/)
			counter++;
		else		{
			counter = 0;
			lastValue = (osl_keys->held.value & STDKEYMASK);
		}
		MyReadKeys();
		oslWaitVSync();
	} while (counter < 30 && (osl_keys->held.value & STDKEYMASK));

end:
	CloseWindow(menuCurrentWindow, 0);
	return lastValue;
}

int fctMainCtrlRedefine(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	if (event == EVENT_INIT)			{
		int i;
		for (i=0;i<NBRE_TOUCHES;i++)
			menuGetKeyName(GetSubMenuItemByInt(menu, i)->value, 20, "/", menuConfig.ctrl.akeys[i]);
	}
	if (event == EVENT_SELECT)		{
/*		//Récupère la touche
		int result = menuRedefineGetNewKey(sub), i;
		//Vérifie que ce n'est pas la même qu'une autre!
		for (i=0;i<NBRE_TOUCHES;i++)	{
			if (menuConfig.ctrl.akeys[i] == result)
				break;
		}
		if (i >= NBRE_TOUCHES)
			menuConfig.ctrl.akeys[sub->prop1] = result;
		else if (i != sub->prop1)		{
			//La touche existe déjà
			sprintf(menuTempMessageText, "Sorry, this key is already used for \'%s\'!", GetSubMenuItemByInt(menu, i)->name);
			MessageBoxTemp(menuTempMessageText, "Error", MB_NONE, 200, 120);
		}*/
		menuConfig.ctrl.akeys[sub->prop1] = menuRedefineGetNewKey(sub);

		//Met à jour le menu
		fctMainCtrlRedefine(menu, sub, EVENT_INIT);
	}
	if (event == EVENT_DEFAULT)			{
		if (menuConfig.ctrl.akeys[sub->prop1] != menuConfigDefault->ctrl.akeys[sub->prop1])			{
			menuConfig.ctrl.akeys[sub->prop1] = menuConfigDefault->ctrl.akeys[sub->prop1];
			fctMainCtrlRedefine(menu, sub, EVENT_INIT);
			MessageBoxTemp("Default key reverted.", "Information", MB_NONE, 140, 90);
		}
	}
	return 1;
}

int fctMainCtrlShortcuts(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	if (event == EVENT_INIT)			{
		int i;
		for (i=0;i<NBRE_CUTS;i++)
			menuGetKeyName(GetSubMenuItemByInt(menu, i)->value, 20, "+", menuConfig.ctrl.acuts[i]);
	}
	if (event == EVENT_SELECT)		{
		//Récupère la touche
		int result = menuRedefineGetNewKey(sub), i;
		//Vérifie que ce n'est pas la même qu'une autre!
		for (i=0;i<NBRE_TOUCHES;i++)	{
			if (menuConfig.ctrl.acuts[i] == result)
				break;
		}

		//Le seul cas où il est permis d'avoir deux fois la même touche: (none) ;)
		if (i >= NBRE_TOUCHES || !result)
			menuConfig.ctrl.acuts[sub->prop1] = result;
		//La touche existe déjà (faisons attention à ce que ce ne soit pas le même raccourci)
		else if (i != sub->prop1)		{
			sprintf(menuTempMessageText, "This key was already used for \'%s\'!\nThey have been exchanged.", GetSubMenuItemByInt(menu, i)->name);
			MessageBoxTemp(menuTempMessageText, "Error", MB_NONE, 230, 120);
			//On échange les touches dans ce cas
			menuConfig.ctrl.acuts[i] = menuConfig.ctrl.acuts[sub->prop1];
			menuConfig.ctrl.acuts[sub->prop1] = result;
		}

		//Met à jour le menu
		fctMainCtrlShortcuts(menu, sub, EVENT_INIT);
	}
	if (event == EVENT_DEFAULT)			{
		if (menuConfig.ctrl.acuts[sub->prop1] != menuConfigDefault->ctrl.acuts[sub->prop1])			{
			menuConfig.ctrl.acuts[sub->prop1] = menuConfigDefault->ctrl.acuts[sub->prop1];
			fctMainCtrlShortcuts(menu, sub, EVENT_INIT);
			MessageBoxTemp("Default key reverted.", "Information", MB_NONE, 140, 90);
		}
	}
	return 1;
}

int fctMainCtrlAnalogToDPad(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_CTRL, &menuConfig.ctrl.analog.toPad);
}


extern SUBMENU menuMainSmsTweak, menuMainMisc, menuMainSmsTweakZ80;
extern SUBMENU menuMainFile;
extern SUBMENU menuMainMiscPspClockNumeric, menuMainVideoGammaNumeric;

SUBMENUITEM menuMainMiscPspClockItems[]=		{
	{"100 MHz", "", "", NULL, 100},
	{"133 MHz", "", "", NULL, 133},
	{"150 MHz", "", "", NULL, 150},
	{"166 MHz", "", "", NULL, 166},
	{"180 MHz", "", "", NULL, 180},
	{"200 MHz", "", "", NULL, 200},
	{"222 MHz", "", "", NULL, 222},
	{"266 MHz", "", "", NULL, 266},
	{"300 MHz", "", "", NULL, 300},
	{"333 MHz", "", "", NULL, 333},
	{"Other...", "", "", &menuMainMiscPspClockNumeric, MASK_DEFAULT_MENU_VALUE},
};

int fctMainVideoTurboSkip(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_VIDEO, &menuConfig.video.turboSkip);
}

SUBMENUMINMAX menuMainVideoTurboSkipNumericMinMax =		{
	0, 50,
	2,
};

SUBMENU menuMainVideoTurboSkipNumeric=		{
	NULL, 1,
	3,			//Numeric
	(SUBMENUITEM*)&menuMainVideoTurboSkipNumericMinMax,
	fctMainVideoTurboSkip
};

SUBMENUITEM menuMainVideoTurboSkipItems[]=		{
	{"0 frame", "", "", NULL, 0},
	{"2 frames", "", "", NULL, 2},
	{"5 frames", "", "", NULL, 5},
	{"15 frames", "", "", NULL, 15},
	{"Other...", "", "", &menuMainVideoTurboSkipNumeric, MASK_DEFAULT_MENU_VALUE},
};

SUBMENUITEM menuMainVideoCountryItems[]=		{
	{"NTSC", "", "", NULL, 0},
	{"PAL", "", "", NULL, 1},
};

SUBMENUITEM menuMainVideoScalingItems[]=		{
	{"1x", "", "", NULL, 0},
	{"1.5x", "", "", NULL, 5},
	{"2x", "", "", NULL, 1},
	{"Fit", "", "", NULL, 2},
	{"Wide", "", "", NULL, 3},
	{"Fullscreen", "", "", NULL, 4},
};

SUBMENUITEM menuMainVideoFrameskipItems[]=		{
	{"0", "", "", NULL, 1},
	{"1", "", "", NULL, 2},
	{"2", "", "", NULL, 3},
	{"3", "", "", NULL, 4},
	{"4", "", "", NULL, 5},
	{"5", "", "", NULL, 6},
	{"6", "", "", NULL, 7},
	{"7", "", "", NULL, 8},
	{"8", "", "", NULL, 9},
	{"9", "", "", NULL, 10},
};

SUBMENUITEM menuMainVideoFrameskipMaxItems[]=		{
	{"0", "", "", NULL, 1},
	{"1", "", "", NULL, 2},
	{"2", "", "", NULL, 3},
	{"3", "", "", NULL, 4},
	{"4", "", "", NULL, 5},
	{"5", "", "", NULL, 6},
	{"6", "", "", NULL, 7},
	{"7", "", "", NULL, 8},
	{"8", "", "", NULL, 9},
	{"9", "", "", NULL, 10},
};

SUBMENUITEM menuMainVideoVSyncItems[]=		{
	{"Auto", "", "", NULL, 0},
//	{"Auto (sync)", "", "", NULL, 1},
	{"On", "", "", NULL, 2},
};

SUBMENUITEM menuMainMiscUIBackgroundItems[]=		{
	{"In-game", "", "", NULL, 0},
	{"Background", "", "", NULL, 1},
};

SUBMENUITEM menuMainVideoGammaItems[]=		{
	{"1.0 (normal)", "", "", NULL, 100},
	{"1.2 (clear)", "", "", NULL, 120},
	{"1.4 (very clear)", "", "", NULL, 140},
	{"1.8 (sun)", "", "", NULL, 180},
	{"Other...", "", "", &menuMainVideoGammaNumeric, MASK_DEFAULT_MENU_VALUE},
};

SUBMENUITEM menuMainOnOffItems[]=		{
	{"Off", "", "", NULL, 0},
	{"On", "", "", NULL, 1},
};

SUBMENUITEM menuMainSoundRateItems[]=		{
	{"44 kHz", "", "", NULL, 44100},
	{"22 kHz", "", "", NULL, 22050},
	{"11 kHz", "", "", NULL, 11025},
};

SUBMENUITEM menuMainSoundStereoItems[]=		{
	{"Stéréo", "", "", NULL, 0},
	{"Mono", "", "", NULL, 1},
};

SUBMENUITEM menuMainSoundBoostItems[]=		{
	{"Off", "", "", NULL, 0},
	{"+1", "", "", NULL, 1},
	{"+2", "", "", NULL, 2},
	{"+3", "", "", NULL, 3},
};

SUBMENUITEM menuMainSoundTurboSoundItems[]=		{
	{"Stop", "", "", NULL, 0},
//	{"Pause", "", "", NULL, 1},
	{"Play", "", "", NULL, 2},
};

SUBMENUITEM menuMainSoundSyncModeItems[]=		{
	{"Buffered", "", "", NULL, 0},
	{"Direct", "", "", NULL, 1},
};


SUBMENUITEM menuMainFileStateItems[]=		{
	{"RAM", "", "", NULL, 10},
	{tmpSlotTextMem[0], "", "", NULL, 0},
	{tmpSlotTextMem[1], "", "", NULL, 1},
	{tmpSlotTextMem[2], "", "", NULL, 2},
	{tmpSlotTextMem[3], "", "", NULL, 3},
	{tmpSlotTextMem[4], "", "", NULL, 4},
	{tmpSlotTextMem[5], "", "", NULL, 5},
	{tmpSlotTextMem[6], "", "", NULL, 6},
	{tmpSlotTextMem[7], "", "", NULL, 7},
	{tmpSlotTextMem[8], "", "", NULL, 8},
	{tmpSlotTextMem[9], "", "", NULL, 9},
//	{"", "", "", NULL, STATE_EXPORT},
};

int fctMainCtrlRedefineAutofireRate(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_CTRL, &menuConfig.ctrl.autofireRate);
}

SUBMENUMINMAX menuMainCtrlRedefineAutofireRateMinMax =		{
	2, 60,
	2,
};

SUBMENU menuMainCtrlRedefineAutofireRate=		{
	NULL, 1,
	3,			//Numeric
	(SUBMENUITEM*)&menuMainCtrlRedefineAutofireRateMinMax,
	fctMainCtrlRedefineAutofireRate
};

SUBMENUITEM menuMainCtrlRedefineItems[]=		{
	{"Up", NULL, "", NULL, 0},
	{"Down", NULL, "", NULL, 1},
	{"Left", NULL, "", NULL, 2},
	{"Right", NULL, "", NULL, 3},

	{"Button 2/B", NULL, "", NULL, 4},
	{"Button 1/A", NULL, "", NULL, 5},
	{"Start", NULL, "", NULL, 6},
	{"Select", NULL, "", NULL, 9},
	{"Autofire 2/B", NULL, "", NULL, 7},
	{"Autofire 1/A", NULL, "", NULL, 8},
	{"Autofire rate", NULL, "", &menuMainCtrlRedefineAutofireRate, 88},
};

SUBMENUITEM menuMainCtrlShortcutsItems[]=		{
	{"Menu", NULL, "", NULL, 0},
	{"Turbo", NULL, "", NULL, 1},
	{"Pause", NULL, "", NULL, 2},
	{"Load state", NULL, "", NULL, 3},
	{"Save state", NULL, "", NULL, 4},
	{"State +", NULL, "", NULL, 5},
	{"State -", NULL, "", NULL, 6},
	{"Reset", NULL, "", NULL, 7},
	{"Music player", NULL, "", NULL, 8},
	{"Screenshot", NULL, "", NULL, 9},
};

int fctMenuMainVideo(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	if (event == EVENT_SELECT)		{
		//
	}
	return 1;
}

extern SUBMENU menuMsgBoxReloadConfig;
void fctMsgBoxReloadConfig_Close(WINDOW *w, int valid);
void fctMsgBoxReloadConfig_Handle(WINDOW *w, int valid);

int fctMenuMainMisc(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	if (event == EVENT_SELECT)		{
		//Reload defaults
		if (sub->prop1 == 77)		{
			SUBMENU *sm = &menuMsgBoxReloadConfig;
			MessageBoxAsync("Reload which configuration?", "Question", MB_OKCANCEL, 200, fctMsgBoxReloadConfig_Close, fctMsgBoxReloadConfig_Handle);
			menuCurrentWindow->hauteur += 36 + HAUTEUR_TEXTE;
			SelectSubMenu(sm, 1);
			sm->active = 1;
			sm->moving = 0;
			sm->close = 0;
		}
		//USB
		else if (sub->prop1 == 3)		{
			if (oslGetUsbState() & PSP_USB_ACTIVATED)			{
				oslStopUsbStorage();
				MessageBoxTemp("USB Storage disabled.", "Information", MB_NONE, 140, 90);
			}
			else		{
				oslStartUsbStorage();
				MessageBoxTemp("USB Storage enabled.", "Information", MB_NONE, 140, 90);
			}
			menuUsbUpdate();
		}
	}
	return 1;
}

//Analog treshold
SUBMENUMINMAX menuMainCtrlAnalogTresholdMinMax =		{
	1, 127,
	3,
};

int fctMainCtrlAnalogTreshold(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_CTRL, &menuConfig.ctrl.analog.treshold);
}

int fctMainCtrlAnalogCalibrationEnabled(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_CTRL, &menuConfig.ctrl.analog.useCalibration);
}

int fctMainCtrlAnalogCalibration(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	if (event == EVENT_SELECT)		{
		if (sub->prop1 == 1)			{
			//Analog calibration
			fadeInit(24, 2);
		}
	}
	return 1;
}

SUBMENU menuMainVideoTurboSkip=		{
	NULL, numberof(menuMainVideoTurboSkipItems),
	2,			//Choice
	menuMainVideoTurboSkipItems,
	fctMainVideoTurboSkip
};

SUBMENU menuMainCtrlAnalogTreshold=		{
	NULL, 1,
	3,			//Numeric
	(SUBMENUITEM*)&menuMainCtrlAnalogTresholdMinMax,
	fctMainCtrlAnalogTreshold
};
//End Analog treshold

SUBMENU menuMainCtrlAnalogToDPad=		{
	NULL, 2,
	2,			//Choice
	menuMainOnOffItems,
	fctMainCtrlAnalogToDPad
};

SUBMENU menuMainCtrlAnalogCalibrationEnabled=		{
	NULL, 2,
	2,			//Choice
	menuMainOnOffItems,
	fctMainCtrlAnalogCalibrationEnabled
};

SUBMENUITEM menuMainCtrlAnalogCalibrationItems[]=		{
	{"Analog to D-Pad", "Analog stick will be used as a pad", "", &menuMainCtrlAnalogToDPad},
	{"Stick treshold", "How far you have to press for a direction to be detected", "", &menuMainCtrlAnalogTreshold},
	{"Enable calibration", NULL, "", &menuMainCtrlAnalogCalibrationEnabled},
	{"Calibrate", "Calibrate according to your joystick", "", NULL, 1},
};

SUBMENU menuMainMiscPspClock=		{
	NULL, numberof(menuMainMiscPspClockItems),
	2,			//Choice
	menuMainMiscPspClockItems,
	fctMainMiscPspClock
};

SUBMENUMINMAX menuMainMiscPspClockNumericMinMax =		{
	100, 333,
	3,
};

SUBMENU menuMainMiscPspClockNumeric=		{
	NULL, 7,
	3,			//Numeric
	(SUBMENUITEM*)&menuMainMiscPspClockNumericMinMax,
	fctMainMiscPspClockNumeric
};

SUBMENU menuMainFileStateLoad=		{
	"RAM slot is temporary, and will be deleted when the console is turned off. Others are written on MS.", numberof(menuMainFileStateItems),
	2,			//Choice
	menuMainFileStateItems,
	fctMainFileStateLoad
};

SUBMENU menuMainFileStateSave=		{
	"RAM slot is temporary, and will be deleted when the console is turned off. Others are written on MS.", numberof(menuMainFileStateItems),
	2,			//Choice
	menuMainFileStateItems,
	fctMainFileStateSave
};

SUBMENU menuMainFileStateDelete=		{
	"RAM slot is temporary, and will be deleted when the console is turned off. Others are written on MS.", numberof(menuMainFileStateItems),
	2,			//Choice
	menuMainFileStateItems,
	fctMainFileStateDelete
};

SUBMENU menuMainVideoCountry=		{
	"PAL = Europe\nNTSC = USA / Japan", 2,
	2,			//Choice
	menuMainVideoCountryItems,
	fctMainVideoCountry
};

SUBMENU menuMainVideoScaling=		{
	NULL, numberof(menuMainVideoScalingItems),
	2,			//Choice
	menuMainVideoScalingItems,
	fctMainVideoScaling
};

SUBMENU menuMainVideoSmoothing=		{
	NULL, 2,
	2,			//Choice
	menuMainOnOffItems,
	fctMainVideoSmoothing
};

SUBMENU menuMainVideoFrameskip=		{
	NULL, 10,
	2,			//Choice
	menuMainVideoFrameskipItems,
	fctMainVideoFrameskip
};

SUBMENU menuMainVideoFrameskipMax=		{
	NULL, 10,
	2,			//Choice
	menuMainVideoFrameskipMaxItems,
	fctMainVideoFrameskipMax
};

SUBMENU menuMainVideoVSync=		{
	NULL, numberof(menuMainVideoVSyncItems),
	2,			//Choice
	menuMainVideoVSyncItems,
	fctMainVideoVSync
};

SUBMENU menuMainMiscUIBackground=		{
	NULL, 2,
	2,			//Choice
	menuMainMiscUIBackgroundItems,
	fctMainMiscUIBackground
};

SUBMENUMINMAX menuMainVideoGammaNumericMinMax =		{
	10, 999,
	3,
};

SUBMENU menuMainVideoGammaNumeric=		{
	NULL, 5,
	3,			//Numeric
	(SUBMENUITEM*)&menuMainVideoGammaNumericMinMax,
	fctMainVideoGamma
};

SUBMENU menuMainVideoGamma=		{
	NULL, 5,
	2,			//Choice
	menuMainVideoGammaItems,
	fctMainVideoGamma
};

SUBMENUITEM menuMainVideoCpuTimeItems[]=		{
	{"Off", "", "", NULL, 0},
	{"CPU usage", "", "", NULL, 1},
	{"Framerate", "", "", NULL, 2},
};

SUBMENU menuMainVideoCpuTime=		{
	NULL, 3,
	2,			//Choice
	menuMainVideoCpuTimeItems,
	fctMainVideoCpuTime
};

SUBMENU menuMainSoundEnabled=		{
	NULL, 2,
	2,			//Choice
	menuMainOnOffItems,
	fctMainSoundEnabled
};

SUBMENU menuMainSoundRate=		{
	NULL, 3,
	2,			//Choice
	menuMainSoundRateItems,
	fctMainSoundRate
};

SUBMENU menuMainSoundStereo=		{
	NULL, 2,
	2,			//Choice
	menuMainSoundStereoItems,
	fctMainSoundStereo
};

SUBMENU menuMainSoundBoost=		{
	NULL, 4,
	2,			//Choice
	menuMainSoundBoostItems,
	fctMainSoundBoost
};

SUBMENU menuMainSoundTurboSound=		{
	NULL, numberof(menuMainSoundTurboSoundItems),
	2,			//Choice
	menuMainSoundTurboSoundItems,
	fctMainSoundTurboSound
};

SUBMENU menuMainSoundSyncMode=		{
	NULL, numberof(menuMainSoundSyncModeItems),
	2,			//Choice
	menuMainSoundSyncModeItems,
	fctMainSoundSyncMode
};



SUBMENU menuMainCtrlRedefine=		{
	"Redefine game keys", numberof(menuMainCtrlRedefineItems),
	1,			//Choice
	menuMainCtrlRedefineItems,
	fctMainCtrlRedefine
};

SUBMENU menuMainCtrlShortcuts=		{
	"Redefine menu shortcuts", numberof(menuMainCtrlShortcutsItems),
	1,			//Choice
	menuMainCtrlShortcutsItems,
	fctMainCtrlShortcuts
};

SUBMENU menuMainCtrlAnalogCalibration=		{
	"Analog configuration", numberof(menuMainCtrlAnalogCalibrationItems),
	1,			//Choice
	menuMainCtrlAnalogCalibrationItems,
	fctMainCtrlAnalogCalibration
};


int fctMainFileSramAutosave(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_FILE, &menuConfig.file.sramAutosave);
}

SUBMENU menuMainFileSramAutosave=		{
	NULL, numberof(menuMainOnOffItems),
	2,			//Choice
	menuMainOnOffItems,
	fctMainFileSramAutosave
};

int fctMainFileSram(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	if (event == EVENT_SELECT)		{
		//Save SRAM
		if (sub->prop1 == 1)
			machine_manage_sram(SRAM_SAVE, 1);
	}
}

SUBMENUITEM menuMainFileSramItems[]=		{
	{"Autosave SRAM", "Save SRAM when quitting a game", "", &menuMainFileSramAutosave},
	{"Save SRAM", "Save SRAM now", "", NULL, 1},
};

SUBMENU menuMainFileSram=		{
	"File > SRAM", numberof(menuMainFileSramItems),
	1,			//Menu
	menuMainFileSramItems,
	fctMainFileSram
};

int fctMainMiscCommands(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	if (event == EVENT_SELECT)		{
		OuvreFichierConfig("plugins.ini", sub->name);
		osl_keys->pressed.circle = 1;
	}
}

SUBMENU menuMainMiscCommands=		{
	"Script plugins", 0,
	1,			//Menu
	NULL,
	fctMainMiscCommands
};

//Music player
int fctMenuMainFileMusicFrequency(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_FILE, &menuConfig.music.frequency);
}

SUBMENUMINMAX menuMainFileMusicFrequencyNumericMinMax =		{
	8000, 44100,
	5,
};

SUBMENU menuMainFileMusicFrequencyNumeric=		{
	NULL, 1,
	3,			//Numeric
	(SUBMENUITEM*)&menuMainFileMusicFrequencyNumericMinMax,
	fctMenuMainFileMusicFrequency
};

SUBMENUITEM menuMainFileMusicFrequencyItems[]=		{
	{"11025 Hz", NULL, "", NULL, 11025},
	{"22050 Hz", NULL, "", NULL, 22050},
	{"33075 Hz", NULL, "", NULL, 33075},
	{"44100 Hz", NULL, "", NULL, 44100},
	{"Other...", "", "", &menuMainFileMusicFrequencyNumeric, MASK_DEFAULT_MENU_VALUE},
};

SUBMENU menuMainFileMusicFrequency=		{
	NULL, numberof(menuMainFileMusicFrequencyItems),
	2,			//Normal
	menuMainFileMusicFrequencyItems,
	fctMenuMainFileMusicFrequency
};

int fctMenuMainFileMusicRepeat(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_FILE, &menuConfig.music.repeatMode);
}

SUBMENUITEM menuMainFileMusicRepeatItems[]=		{
	{"Play once", NULL, "", NULL, 0},
	{"Repeat one", NULL, "", NULL, 1},
	{"Repeat all", NULL, "", NULL, 2},
	{"Shuffle repeatedly", NULL, "", NULL, 5},
};

SUBMENU menuMainFileMusicRepeat=		{
	NULL, numberof(menuMainFileMusicRepeatItems),
	2,			//Normal
	menuMainFileMusicRepeatItems,
	fctMenuMainFileMusicRepeat
};

int fctMainFileMusic(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	if (event == EVENT_SELECT)		{
		//Load GYM
		if (sub->prop1 == 1)			{
			fadeInit(24, 3);
		}
		//Unload
		if (sub->prop1 == 2)		{
			menuMusicStop();
			musicEnabled = 0;
			menuConfig.music.filename[0] = 0;
		}
	}
}

SUBMENUITEM menuMainFileMusicItems[]=		{
	{"Load music", "Load a ZIP playlist. Must only contain *.GYM or *.VGM files.", "", NULL, 1},
	{"Frequency", "Set the sample rate for the music player. Lower is the fastest but also worst.", "", &menuMainFileMusicFrequency},
	{"Repeat", "Set the repeat mode", "", &menuMainFileMusicRepeat},
	{"Stop music", "Unload and stop music", "", NULL, 2},
//	{"Looping music", "If enabled, music will be looped, else the next track will be played.", "", &menuMainFileMusic},
};

SUBMENU menuMainFileMusic=		{
	"Music Player", numberof(menuMainFileMusicItems),
	1,			//Menu
	menuMainFileMusicItems,
	fctMainFileMusic
};

//End music player

//Mettre un lien vers un menu principal est interdit!!! Mettre un lien vers le menu parent le fermera (autre parent non supporté).
SUBMENUITEM menuMainFileItems[]=		{
	{"Load ROM", "Load and start a new game", "", NULL, 1},
	{"Reset game", NULL, "", NULL, 2},
	{"Screenshot", "Take a screenshot, and places it into Psp/Game/Photo/MasterBoy/GameName_xx.png.", "", NULL, 5},
	{"About MasterBoy", "About MasterBoy", "", NULL, 4},
	{"Exit", "Return to the PSP menu", "", NULL, 3},
};

SUBMENUITEM menuMainStateItems[]=		{
	{"State load", "Load a previously saved game state", "", &menuMainFileStateLoad},
	{"State save", "Save current game state", "", &menuMainFileStateSave},
	{"State delete", "Delete a saved game state", "", &menuMainFileStateDelete},
	{"Manage SRAM", "Manage backup memory", "", &menuMainFileSram},
};

int fctMenuMainVideoBrightnessMode(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_VIDEO, &menuConfig.video.brightnessMode);
}


SUBMENUITEM menuMainVideoBrightnessModeItems[]=		{
	{"Normal", NULL, "", NULL, 0},
	{"Bright & Fast", NULL, "", NULL, 1},
	{"Invert", NULL, "", NULL, 3},
};

SUBMENU menuMainVideoBrightnessMode=		{
	NULL, numberof(menuMainVideoBrightnessModeItems),
	2,			//Normal
	menuMainVideoBrightnessModeItems,
	fctMenuMainVideoBrightnessMode
};


int fctMainVideoVibrance(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_VIDEO, &menuConfig.video.vibrance);
}

SUBMENUMINMAX menuMainVideoVibranceNumericMinMax =		{
	0, 255,
	3,
};

SUBMENU menuMainVideoVibranceNumeric=		{
	NULL, 1,
	3,			//Numeric
	(SUBMENUITEM*)&menuMainVideoVibranceNumericMinMax,
	fctMainVideoVibrance
};

SUBMENUITEM menuMainVideoVibranceItems[]=		{
	{"Grayscale (0)", NULL, "", NULL, 0},
	{"Washed out (96)", NULL, "", NULL, 96},
	{"Normal (128)", NULL, "", NULL, 128},
	{"Vivid (160)", NULL, "", NULL, 160},
	{"Other...", "", "", &menuMainVideoVibranceNumeric, MASK_DEFAULT_MENU_VALUE},
};

SUBMENU menuMainVideoVibrance=		{
	NULL, numberof(menuMainVideoVibranceItems),
	2,			//Normal
	menuMainVideoVibranceItems,
	fctMainVideoVibrance
};


int fctMenuMainVideoRenderDepth(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_VIDEO, &menuConfig.video.renderDepth);
}


SUBMENUITEM menuMainVideoRenderDepthItems[]=		{
	{"8 bits", NULL, "", NULL, 8},
	{"16 bits", NULL, "", NULL, 16},
};

SUBMENU menuMainVideoRenderDepth=		{
	NULL, numberof(menuMainVideoRenderDepthItems),
	2,			//Normal
	menuMainVideoRenderDepthItems,
	fctMenuMainVideoRenderDepth
};

int fctMenuMainVideoRenderLeftBar(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_VIDEO, &menuConfig.video.leftBar);
}

SUBMENUITEM menuMainVideoRenderLeftBarItems[]=		{
	{"Hidden", NULL, "", NULL, 0},
	{"Shown", NULL, "", NULL, 1},
	{"Auto (default)", NULL, "", NULL, 2},
};

SUBMENU menuMainVideoRenderLeftBar=		{
	NULL, numberof(menuMainVideoRenderLeftBarItems),
	2,			//Normal
	menuMainVideoRenderLeftBarItems,
	fctMenuMainVideoRenderLeftBar
};

int fctMenuMainVideoRenderSpriteLimit(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_VIDEO, &menuConfig.video.spriteLimit);
}

SUBMENU menuMainVideoRenderSpriteLimit=		{
	NULL, numberof(menuMainOnOffItems),
	2,			//Normal
	menuMainOnOffItems,
	fctMenuMainVideoRenderSpriteLimit
};

int fctMenuMainVideoSyncMode(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_VIDEO, &menuConfig.video.syncMode);
}

int fctMenuMainVideoGBColorization(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_VIDEO, &menuConfig.gameboy.colorization);
}

int fctMenuMainVideoGBType(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_VIDEO, &menuConfig.gameboy.gbType);
}

int fctMenuMainVideoGB(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	if (event == EVENT_SELECT)		{
		if (sub->prop1 == 1)		{
			menuShowPaletteList();
		}
	}
	return 1;
}


SUBMENUITEM menuMainVideoSyncModeItems[]=		{
	{"Loose", NULL, "", NULL, 1},
	{"Tight", NULL, "", NULL, 0},
};

SUBMENUITEM menuMainVideoGBTypeItems[]=		{
	{"Auto select", NULL, "", NULL, 0},
	{"Game Boy (DMG)", NULL, "", NULL, 1},
	{"Super Game Boy", NULL, "", NULL, 2},
	{"Game Boy Color", NULL, "", NULL, 3},
	{"Game Boy Advance", NULL, "", NULL, 4},
};

SUBMENU menuMainVideoSyncMode=		{
	NULL, numberof(menuMainVideoSyncModeItems),
	2,			//Normal
	menuMainVideoSyncModeItems,
	fctMenuMainVideoSyncMode
};

SUBMENU menuMainVideoGBColorization =		{
	NULL, numberof(menuMainOnOffItems),
	2,			//Normal
	menuMainOnOffItems,
	fctMenuMainVideoGBColorization
};

SUBMENU menuMainVideoGBType =		{
	NULL, numberof(menuMainVideoGBTypeItems),
	2,			//Normal
	menuMainVideoGBTypeItems,
	fctMenuMainVideoGBType
};

SUBMENUITEM menuMainVideoRenderItems[]=		{
	{"Scaling", "Allows to set the display size", "", &menuMainVideoScaling},
	{"Smoothing", "Soften the rendering; should be enabled as it costs nothing", "", &menuMainVideoSmoothing},
	{"Depth", "Set the render depth; HBL palette processing is only available in 16-bit mode. Much slower", "", &menuMainVideoRenderDepth},
	{"Brightness mode", "Set the brightess mode.", "", &menuMainVideoBrightnessMode},
	{"Gamma correction", "Set to an higher value when you're outside.", "", &menuMainVideoGamma},
	{"Color vibrance", "Increases the render vibrance and flashiness. Any other mode than normal is slower.", "", &menuMainVideoVibrance},
	{"Left bar", "SMS only: displays the 8-pixel column to the left of the screen. Most games don't use it.", "", &menuMainVideoRenderLeftBar},
	{"Sprite limit", "Disabling it reduces flickering when too many sprites are on the screen. Real console: On.", "", &menuMainVideoRenderSpriteLimit},
};

SUBMENUITEM menuMainVideoSyncItems[]=		{
	{"Frameskip", "Avoids slow downs", "", &menuMainVideoFrameskip},
	{"Max Frameskip", NULL, "", &menuMainVideoFrameskipMax},
	{"VSync", "Auto means enabled only when not lagging", "", &menuMainVideoVSync},
	{"Sync mode", "Set to tight if your game can often slow down. Set to loose for more speed.", "", &menuMainVideoSyncMode},
};

SUBMENUITEM menuMainVideoMiscItems[]=		{
	{"Country", "Set PAL or NTSC", "", &menuMainVideoCountry},
	{"Turbo skip", "How many frames to skip rendering in turbo mode", "", &menuMainVideoTurboSkip},
	{"Show framerate", "Shows the CPU time used per frame or the framerate (fps)", "", &menuMainVideoCpuTime},
};

SUBMENUITEM menuMainVideoGBItems[]=		{
	{"Palette", "Browse predefined palettes (palettes.ini).", "", NULL, 1},
	{"Machine type", "Set the emulated game boy model. Needs a reset to take effect.", "", &menuMainVideoGBType},
	{"Colorization", "Enable game-specific colorization functionality.", "", &menuMainVideoGBColorization},
};

SUBMENU menuMainVideoMisc=		{
	"Video > Misc", numberof(menuMainVideoMiscItems),
	1,			//Normal
	menuMainVideoMiscItems,
	fctMenuMainVideo
};

SUBMENU menuMainVideoSync=		{
	"Video > Synchro", numberof(menuMainVideoSyncItems),
	1,			//Normal
	menuMainVideoSyncItems,
	fctMenuMainVideo
};

SUBMENU menuMainVideoRender=		{
	"Video > Render", numberof(menuMainVideoRenderItems),
	1,			//Normal
	menuMainVideoRenderItems,
	fctMenuMainVideo
};

SUBMENU menuMainVideoGB=		{
	"Video > Game Boy", numberof(menuMainVideoGBItems),
	1,			//Normal
	menuMainVideoGBItems,
	fctMenuMainVideoGB
};

SUBMENUITEM menuMainVideoItems[]=		{
	{"Render", "Display options", ">>", &menuMainVideoRender},
	{"Synchronization", "Frameskip, vsync, etc.", ">>", &menuMainVideoSync},
	{"Game Boy", "Options specific to the Game Boy.", ">>", &menuMainVideoGB},
	{"Misc", "Misc. video options", ">>", &menuMainVideoMisc},
};

SUBMENUITEM menuMainSoundItems[]=		{
	{"Enabled", NULL, "", &menuMainSoundEnabled},
	{"Sample rate", "The highest it is, the better the sound is (slower).", "", &menuMainSoundRate},
	{"Output mode", "Mono or stereo sound output.", "", &menuMainSoundStereo},
	{"Volume boost", NULL, "", &menuMainSoundBoost},
	{"Sync mode", "Set the sound synchronization mode. Refer to the README for information.", "", &menuMainSoundSyncMode},
	{"Turbo mode", "Set the sound behavior in turbo mode.", "", &menuMainSoundTurboSound},

	//PADDING
/*	{"pad1", NULL, "", &menuMainSoundBoost},
	{"pad2", NULL, "", &menuMainSoundBoost},
	{"pad3", NULL, "", &menuMainSoundBoost},
	{"pad4", NULL, "", &menuMainSoundBoost},
	{"pad5", NULL, "", &menuMainSoundBoost},
	{"pad6", NULL, "", &menuMainSoundBoost},
	{"pad7", NULL, "", &menuMainSoundBoost},
	{"pad8", NULL, "", &menuMainSoundBoost},
	{"pad1", NULL, "", &menuMainSoundBoost},
	{"pad2", NULL, "", &menuMainSoundBoost},
	{"pad3", NULL, "", &menuMainSoundBoost},
	{"pad4", NULL, "", &menuMainSoundBoost},
	{"pad5", NULL, "", &menuMainSoundBoost},
	{"pad6", NULL, "", &menuMainSoundBoost},
	{"pad7", NULL, "", &menuMainSoundBoost},
	{"pad8", NULL, "", &menuMainSoundBoost},
	{"pad1", NULL, "", &menuMainSoundBoost},
	{"pad2", NULL, "", &menuMainSoundBoost},
	{"pad3", NULL, "", &menuMainSoundBoost},
	{"pad4", NULL, "", &menuMainSoundBoost},
	{"pad5", NULL, "", &menuMainSoundBoost},
	{"pad6", NULL, "", &menuMainSoundBoost},
	{"pad7", NULL, "", &menuMainSoundBoost},
	{"pad8", NULL, "", &menuMainSoundBoost},
	{"pad1", NULL, "", &menuMainSoundBoost},
	{"pad2", NULL, "", &menuMainSoundBoost},
	{"pad3", NULL, "", &menuMainSoundBoost},
	{"pad4", NULL, "", &menuMainSoundBoost},
	{"pad5", NULL, "", &menuMainSoundBoost},
	{"pad6", NULL, "", &menuMainSoundBoost},
	{"pad7", NULL, "", &menuMainSoundBoost},
	{"pad8", NULL, "", &menuMainSoundBoost},
	{"pad1", NULL, "", &menuMainSoundBoost},
	{"pad2", NULL, "", &menuMainSoundBoost},
	{"pad3", NULL, "", &menuMainSoundBoost},
	{"pad4", NULL, "", &menuMainSoundBoost},
	{"pad5", NULL, "", &menuMainSoundBoost},
	{"pad6", NULL, "", &menuMainSoundBoost},
	{"pad7", NULL, "", &menuMainSoundBoost},
	{"pad8", NULL, "", &menuMainSoundBoost},
	{"pad1", NULL, "", &menuMainSoundBoost},
	{"pad2", NULL, "", &menuMainSoundBoost},
	{"pad3", NULL, "", &menuMainSoundBoost},
	{"pad4", NULL, "", &menuMainSoundBoost},
	{"pad5", NULL, "", &menuMainSoundBoost},
	{"pad6", NULL, "", &menuMainSoundBoost},
	{"pad7", NULL, "", &menuMainSoundBoost},
	{"pad8", NULL, "", &menuMainSoundBoost},
	{"pad1", NULL, "", &menuMainSoundBoost},
	{"pad2", NULL, "", &menuMainSoundBoost},
	{"pad3", NULL, "", &menuMainSoundBoost},
	{"pad4", NULL, "", &menuMainSoundBoost},
	{"pad5", NULL, "", &menuMainSoundBoost},
	{"pad6", NULL, "", &menuMainSoundBoost},
	{"pad7", NULL, "", &menuMainSoundBoost},
	{"pad8", NULL, "", &menuMainSoundBoost},*/
};

SUBMENUITEM menuMainCtrlItems[]=		{
	{"Redefine", "Redefine SMS / GG controls", ">>", &menuMainCtrlRedefine},
	{"Shortcuts", "Various in-game commands", ">>", &menuMainCtrlShortcuts},
	{"Analog configuration", "Configure your joystick for a better playability", ">>", &menuMainCtrlAnalogCalibration},
};

SUBMENUITEM menuMainMiscUIItems[]=		{
	{"Background", "Set what you want as this menu's background", "", &menuMainMiscUIBackground},
	{"USB connection", "Toggle (on/off) USB storage state.", "", NULL, 3},
	{"Reload defaults", "Reload default configuration", "", NULL, 77},
};

SUBMENU menuMainMiscUI=		{
	"Misc > User interface", numberof(menuMainMiscUIItems),
	1,			//Normal
	menuMainMiscUIItems,
	fctMenuMainMisc
};

SUBMENUITEM menuMainMiscItems[]=		{
	{"PSP CPU clock", "Runs faster but drains more battery", "", &menuMainMiscPspClock},
	{"Z80 clock", "Speed of the emulated CPU. Prevents slowdowns in some games but needs more power.", "", &menuMainSmsTweakZ80},
//	{"SMS hard tweaking", "Can improve playability on some games", ">>", &menuMainSmsTweak},
	{"User interface", "Set options regarding the user interface", ">>", &menuMainMiscUI},
	{"Script plugins", "Special script plugins (plugins.ini)", ">>", &menuMainMiscCommands, 88},
};

int fctMainSmsTweakZ80(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	return GestionEventStandardInt(menu, sub, event, MENUUPD_MISC, &menuConfig.misc.z80Clock);
}

SUBMENUMINMAX menuMainSmsTweakZ80NumericMinMax =		{
	1, 400,
	3,
};

SUBMENU menuMainSmsTweakZ80Numeric=		{
	NULL, 1,
	3,			//Numeric
	(SUBMENUITEM*)&menuMainSmsTweakZ80NumericMinMax,
	fctMainSmsTweakZ80
};

SUBMENUITEM menuMainSmsTweakZ80Items[]=		{
	{"50%", "", "", NULL, 50},
	{"75%", "", "", NULL, 75},
	{"100% (normal)", "", "", NULL, 100},
	{"110%", "", "", NULL, 110},
	{"125%", "", "", NULL, 125},
	{"150%", "", "", NULL, 150},
	{"200%", "", "", NULL, 200},
	{"Other...", "", "", &menuMainSmsTweakZ80Numeric, MASK_DEFAULT_MENU_VALUE},
};

SUBMENU menuMainSmsTweakZ80=		{
	NULL, numberof(menuMainSmsTweakZ80Items),
	2,			//Choice
	menuMainSmsTweakZ80Items,
	fctMainSmsTweakZ80
};

/*SUBMENUITEM menuMainSmsTweakItems[]=		{
	{"Z80 clock", "Prevent slowdowns in some games but needs more power", "", &menuMainSmsTweakZ80},
//	{"SMS lines", "Lines displayed par frame", "Normal", NULL},
	{"Return", "Back to previous menu", "<<", &menuMainMisc},
};*/

void fctMsgBoxAbout_Close(WINDOW *w, int valid)		{

}

int msgAboutTextOffset = 0;
float msgAboutTextOffsetDisplay = 0;
int msgAboutTextHeight = 0;
int msgAboutArrowAlpha[2];
u32 msgBoxArrowOffset;

void fctMsgBoxAbout_Handle(WINDOW *w, int event)		{
	const char *cAboutText = "MasterBoy by Brunni\n"
		"\n"
		"MasterBoy is a Game Boy and Sega Master System emulator for the PSP. "
		"It is coded in C and based off both the original source of SMSPlus by Charles Macdonald and TGB Dual by Hii.\n"
		"It is based on MMSPlus, a Master System emulator by me, so I will never be able to thank enough people who helped me for this project.\n"
		"Please visit the MasterBoy webpage on my website:\n"
		"http://brunni.dev-fr.org/\n\n"
		"Proper credits and thanks must be given to the following people :\n"
		" - Hii for the original TGB emulator.\n"
		" - Mr. Mirakichi for creating RIN. His sources were very useful to me and he really desserves credits.\n"
		" - DJP at playeradvance.org for his excellent game boy background.\n"
		" - Charles Macdonald the creator of SMSPlus for PC.\n"
		" - Cswindle for his original port of SMSPlus to the PSP.\n"
		" - Maxim for his Winamp VGM Plugin, his detailed VGM documentation and all the effort that has gone into his work.\n"
		" - Googer for his foobar GYM plugin.\n"
		" - Phoebius for his beautiful icons.\n"
		" - Japi for his amazing icons and backgrounds.\n"
		"Thanks to Jack Stone and Yodajr for beta testing and always providing positive and constructive feedback for me to build on MMSPlus and MasterBoy with.\n"
		"And anyone else who I have forgotten...";
	if (event == EVENT_INIT)		{
		msgAboutTextOffset = msgAboutTextHeight = msgAboutTextOffsetDisplay = 0;
		msgAboutArrowAlpha[0] = msgAboutArrowAlpha[1] = 0;
		msgBoxArrowOffset = 0;
	}
	if (event == EVENT_DRAW)		{
		int xx0, xx1, yy0, yy1,
			x0, x1, y0, y1, larg, haut;
		int arrow_offset_values[] = {0, 0, 1, 2, 3, 4, 4, 3, 2, 1};
		//Position de la fenêtre (pour clipping)
		larg = (w->largeur * w->multiply) >> 8;
		haut = (w->hauteur * w->multiply) >> 8;
		x0 = (LARG - larg) / 2;
		x1 = x0 + larg;
		y0 = (HAUT - haut) / 2;
		y1 = y0 + haut;
		//Position du texte
		xx0 = (LARG - w->largeur) / 2;
		xx1 = xx0 + w->largeur;
		yy0 = (HAUT - w->hauteur) / 2;
		yy1 = yy0 + w->hauteur;
		//Dessin
		oslSetFont(ftGlow);
		oslSetTextColor(RGB(255, 255, 255));
		mySetScreenClipping(max(xx0 + 4, x0), max(yy0 + 18, y0), min(xx1 - 4, x1), min(yy1 - 20, y1));
		oslDrawTextBox(xx0 + 4, yy0 + 18 - (int)(msgAboutTextOffsetDisplay + 0.5f), xx1 - 4, yy1 - 20 + osl_curFont->charHeight, cAboutText, 0);

		//Sauvegarde l'alpha
		u32 alphaSav, effectSav;
		alphaSav = osl_currentAlphaCoeff, effectSav = osl_currentAlphaEffect;

		//Flèche du haut
		oslSetImageTileSize(imgNumbers, 0, 154, 14, 7);
		oslSetAlpha(OSL_FX_ALPHA|OSL_FX_COLOR, RGBA(255, 255, 80, msgAboutArrowAlpha[0]));
		oslDrawImageXY(imgNumbers, xx1 - 14 - 4, yy0 + 18 + 4 - arrow_offset_values[(msgBoxArrowOffset / 6) % numberof(arrow_offset_values)]);

		//Flèche du bas
		oslSetImageTileSize(imgNumbers, 0, 161, 14, 7);
		oslSetAlpha(OSL_FX_ALPHA|OSL_FX_COLOR, RGBA(255, 255, 80, msgAboutArrowAlpha[1]));
		oslDrawImageXY(imgNumbers, xx1 - 14 - 4, yy1 - 20 - 7 - 4 + arrow_offset_values[(msgBoxArrowOffset / 6) % numberof(arrow_offset_values)]);

		//Restaure l'environnement
		oslSetAlpha(effectSav, alphaSav);
		oslResetScreenClipping();
	}
	else if (event == EVENT_HANDLE)		{
		int xx0, xx1, yy0, yy1, nbLignes;
		xx0 = (LARG - w->largeur) / 2;
		xx1 = xx0 + w->largeur;
		yy0 = (HAUT - w->hauteur) / 2;
		yy1 = yy0 + w->hauteur;
		nbLignes = (yy1 - yy0 - 20 - 18) / osl_curFont->charHeight;
		if (msgAboutTextHeight == 0)
			msgAboutTextHeight = GetTextBoxHeight(xx0 + 4, xx1 - 4, cAboutText);
		if (osl_keys->pressed.up)
			msgAboutTextOffset = max(msgAboutTextOffset - 2, 0);
		if (osl_keys->pressed.down)
			msgAboutTextOffset = min(msgAboutTextOffset + 2, msgAboutTextHeight / osl_curFont->charHeight - nbLignes);
		msgAboutTextOffsetDisplay += ((msgAboutTextOffset * osl_curFont->charHeight) - msgAboutTextOffsetDisplay) / 3.0f;
		
		//Tout en bas
		int arrowHaut, arrowBas;
		arrowBas = !(msgAboutTextOffset == msgAboutTextHeight / osl_curFont->charHeight - nbLignes);
		arrowHaut = !(msgAboutTextOffset == 0);
		if (arrowHaut)
			msgAboutArrowAlpha[0] = min(msgAboutArrowAlpha[0] + 32, 255);
		else
			msgAboutArrowAlpha[0] = max(msgAboutArrowAlpha[0] - 32, 0);
		if (arrowBas)
			msgAboutArrowAlpha[1] = min(msgAboutArrowAlpha[1] + 32, 255);
		else
			msgAboutArrowAlpha[1] = max(msgAboutArrowAlpha[1] - 32, 0);
		
		msgBoxArrowOffset++;

/*		HandleSubMenu(&menuMsgBoxSaveConfig);
		//Ne jamais laisser gérer ça par la boîte sinon elle se fermera!
		osl_keys->pressed.cross = 0;*/
	}
}

void menuTakeScreenshot()		{
	OSL_IMAGE img;
	int imageNumber, i, f;
	char path[MAX_PATH * 2], gamename[MAX_PATH], *ptr, *finalptr;

	//Finalptr contiendra juste le nom de fichier
	ptr = menuConfig.file.filename;
	finalptr = ptr;
	while(*ptr)		{
		if (*ptr == '/' || *ptr == '\\')
			finalptr = ptr + 1;
		ptr++;
	}
	GetJustFileName(gamename, finalptr);	

	sceIoMkdir("ms0:/PSP/PHOTO/MasterBoy", 0);
	for (imageNumber=1;imageNumber<100;imageNumber++)			{
		sprintf(path, "ms0:/PSP/PHOTO/MasterBoy/%s_%02i.png", gamename, imageNumber);
		f = sceIoOpen(path, PSP_O_RDONLY, 0777);
		if (f >= 0)
			sceIoClose(f);
		else		{
			int viewPortX = bitmap.viewport.x, viewPortW = bitmap.viewport.w;
			if (!IS_GG && (((vdp.reg[0] & 0x20) && menuConfig.video.leftBar == 2) || menuConfig.video.leftBar == 0))		{
				bitmap.viewport.x = max(bitmap.viewport.x, 8);
				bitmap.viewport.w -= bitmap.viewport.x - viewPortX;
			}
			//Juste pour le screenshot
			img.offsetX0 = bitmap.viewport.x;
			img.offsetY0 = bitmap.viewport.y;
			img.offsetX1 = bitmap.viewport.x + bitmap.viewport.w;
			img.offsetY1 = bitmap.viewport.y + bitmap.viewport.h;
			img.sizeX = 256;
			img.sizeY = 192;
			img.sysSizeX = 256;
			img.sysSizeY = 192;
			img.realSizeX = 256;
			img.realSizeY = 192;
			img.data = Screen.scrBuffer;
			img.pixelFormat = OSL_PF_5551;
			img.location = OSL_IN_RAM;

			int tmp = bitmap.depth;
			bitmap.depth = 16;
			bitmap.pitch  = 256 * bitmap.depth / 8;
			//Initialise le state temporaire
			if (!menuTempStateMemory)			{
				menuTempStateMemory = malloc(RAM_STATE_SIZE);
				pspSaveState(STATE_TEMP);
			}

			//Génération du screenshot
			machine_frame(0);
			oslWriteImageFilePNG(&img, path, 0);

			//Restauration
			bitmap.viewport.x = viewPortX;
			bitmap.viewport.w = viewPortW;
			bitmap.depth = tmp;
			bitmap.pitch  = 256 * bitmap.depth / 8;
			pspLoadState(STATE_TEMP);
			machine_frame(0);
			break;
		}
	}


}


int fctMenuMainFile(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	if (event == EVENT_SELECT)		{
		if (sub->prop1 == 1)			{
			//Load ROM
			fadeInit(24, 1);
		}
		if (sub->prop1 == 2)			{
			if (menuIsInGame)		{
				if (MessageBox("Do you want to reset the console?", "Question", MB_YESNO))			{
					machine_reset();
					menuPlusTerminated = 1;
				}
			}
		}
		if (sub->prop1 == 3)			{
			if (menuIsInGame)		{
				if (!MessageBox("Do you want to exit the game?", "Question", MB_YESNO))
					return 1;
			}
			//Quitter
			osl_quit = 1;
		}
		if (sub->prop1 == 4)		{
			MessageBoxAsync("", "About MasterBoy...", MB_OKCANCEL, 400, fctMsgBoxAbout_Close, fctMsgBoxAbout_Handle);
			menuCurrentWindow->hauteur += 180;
		}
		if (sub->prop1 == 5)		{
			if (menuIsInGame)
				menuTakeScreenshot();
		}
	}
	return 1;
}

int fctMenuMainState(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	if (event == EVENT_SELECT)		{
		//
	}
	return 1;
}

int fctMenuMainSound(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	if (event == EVENT_SELECT)		{
		//
	}
	return 1;
}

int fctMenuMainCtrl(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	if (event == EVENT_SELECT)		{
	}
	return 1;
}

SUBMENU menuMainFile=		{
	"File", numberof(menuMainFileItems),
	1,			//Normal
	menuMainFileItems,
	fctMenuMainFile
};

SUBMENU menuMainState=		{
	"Save states", numberof(menuMainStateItems),
	1,			//Normal
	menuMainStateItems,
	fctMenuMainState
};

SUBMENU menuMainVideo=		{
	"Video", numberof(menuMainVideoItems),
	1,			//Normal
	menuMainVideoItems,
	fctMenuMainVideo
};

SUBMENU menuMainSound=		{
	"Sound", 6,
	1,			//Normal
	menuMainSoundItems,
	fctMenuMainSound
};

SUBMENU menuMainCtrl=		{
	"Control", numberof(menuMainCtrlItems),
	1,			//Normal
	menuMainCtrlItems,
	fctMenuMainCtrl
};

SUBMENU menuMainMisc=		{
	"Miscenalleous", numberof(menuMainMiscItems),
	1,			//Normal
	menuMainMiscItems,
	fctMenuMainMisc
};

/*SUBMENU menuMainSmsTweak=		{
	"SMS hardware tweaking", numberof(menuMainSmsTweakItems),
	1,			//Normal
	menuMainSmsTweakItems
};*/

MENUITEM menuMainItems[]=		{
	{96, &menuMainFile},
	{192, &menuMainState},
	{64, &menuMainVideo},
	{32, &menuMainSound},
	{0, &menuMainFileMusic},
	{160, &menuMainCtrl},
	{128, &menuMainMisc},
};

MENU menuMain=		{
	numberof(menuMainItems),
	menuMainItems
};

/*WINDOW windowConfirmation =		{
	200, -1,
	"Confirmation",
	"The save state will be written to the memory stick. Are you sure?",
	"X Ok", "O Cancel",
};*/

SUBMENUITEM *GetSubMenuItemByInt(SUBMENU *menu, int value)		{
	int i;
	for (i=0;i<menu->nbItems;i++)		{
		if (menu->items[i].prop1 == value)
			return &menu->items[i];
	}
	return NULL;
}

char SelectSubMenuItemPositionByValueInt(SUBMENU *menu, int param)		{
	int i;
	for (i=0;i<menu->nbItems;i++)		{
		if (menu->items[i].prop1 == param)		{
			SelectSubMenuItemPosition(menu, i);
			return 1;
		}
	}
	return 0;
}

void SelectSubMenuItemPosition(SUBMENU *menu, int pos)
{
	menu->state.selection = pos;
	menu->state.selectionDirect = 1;
}

int SubMenuItemPositionSelected(SUBMENU *menu, SUBMENUITEM *item)
{
	if (menu->fctGestion)
		return menu->fctGestion(menu, item, EVENT_SELECT);
	else
		return 1;
}

int SubMenuItemPositionCanceled(SUBMENU *menu, SUBMENUITEM *item)
{
	if (menu->fctGestion)
		return menu->fctGestion(menu, item, EVENT_CANCEL);
	else
		return 1;
}

void CreateDigitsFromInt(SUBMENUMINMAX *mm, int value, int fix)
{
	int i = mm->width - 1;
	while (i >= 0)		{
		mm->digits[i] = value % 10;
		if (fix)
			mm->digitsPos[i] = (float)(mm->digits[i] * HAUTEUR_DIGIT);
		value /= 10;
		i--;
	}
}

void CreateDigitsFromStr(SUBMENUMINMAX *mm, char *str, int fix)
{
	int i = 0;
	while (i < NB_MAX_DIGITS && str[i])		{
		mm->digits[i] = str[i] - '0';
		mm->digitsPos[i] = (float)(((int)str[i] - '0') * HAUTEUR_DIGIT);
		i++;
	}
}

int GetIntFromDigits(SUBMENUMINMAX *mm)
{
	int i = mm->width - 1, r = 0, v = 1;
	while (i >= 0)		{
		r += mm->digits[i] * v;
		v *= 10;
		i--;
	}
	return r;
}

void SelectSubMenu(SUBMENU *s, char isNew)
{
	if (s->fctGestion)		{
		if (isNew)
			s->fctGestion(s, NULL, EVENT_SET);
		s->fctGestion(s, NULL, EVENT_INIT);
	}
	//Numérique => recopie les digits
	if (s->type == 3)		{
		SUBMENUMINMAX *mm = (SUBMENUMINMAX*)s->items;
		CreateDigitsFromStr(mm, mm->value, 1);
		mm->flecheX = mm->width - 1;
		mm->flechePosX = (float)(mm->flecheX * LARGEUR_DIGIT);
	}
}

void CreeImageBord()		{
	imgBord = oslCreateImage(64, 8, OSL_IN_RAM, OSL_PF_4BIT);
	memcpy(imgBord->data, imgBord_data, 64 * sizeof(u32));
	imgBord->palette = oslCreatePalette(16, OSL_PF_8888);
	((unsigned long*)imgBord->palette->data)[0] = RGBA(0, 0, 0, 0);
	((unsigned long*)imgBord->palette->data)[1] = RGBA(255, 255, 255, 255);
	((unsigned long*)imgBord->palette->data)[2] = RGBA(255, 0, 0, 255);
	oslUncachePalette(imgBord->palette);
	oslUncacheImage(imgBord);
	oslSetImageTile(imgBord, 0, 0, 5, 5);
}

void fadeInit(int value, int reason)		{
	//Rien ne change...
	if (fadeReason == reason)
		return;
	if (value > 0)
		fadeLevel = 0;
	else
		fadeLevel = 255;
	fadeDirection = value;
	fadeReason = reason;
}

void menuSetStatusBarMessageDirect(char *message)		{
	if (!message)
		menuStatusBarMessage = NULL;
	else		{
		if (!menuStatusBarMessage)
			menuStatusBarMessage = message;
	}
//	safe_strcpy(menuStatusBarMessageInd, message, sizeof(menuStatusBarMessageInd));
}

void menuSetStatusBarMessageIndirect(char *message)		{
	if (!message)
		menuStatusBarMessage = NULL;
	else		{
		if (!menuStatusBarMessage)		{
			safe_strcpy(menuStatusBarMessageInd, message, sizeof(menuStatusBarMessageInd));
			menuStatusBarMessage = menuStatusBarMessageInd;
		}
	}
}

/*void DrawDigitMult(int x, int y, int digitNb, int mult)
{
	oslSetImageTileSize(imgNumbers, 0, digitNb * 7, 7, 7);
	imgNumbers->stretchX = 7 * mult;
	imgNumbers->stretchY = 7 * mult;
	oslDrawImageSimpleXY(imgNumbers, x, y);
}*/

/*void DrawFlecheDouble(int x, int y, int digitNb)
{
	oslSetImageTileSize(imgNumbers, 0, (digitNb + 14) * 7, 14, 7);
	oslDrawImageSimpleXY(imgNumbers, x, y);
}*/

void DrawDigitScroll(int x, int y, float fdigit)
{
	const int mult = 1;
	int digit = (int)(fdigit);
	if (digit < 0)
		digit += HAUTEUR_DIGIT * 10;
	int digitNb = digit / HAUTEUR_DIGIT;
	digitNb = mod(digitNb, 10);
	oslSetImageTileSize(imgNumbers, 0, digitNb * 15, 14, 14);
	imgNumbers->stretchX = 14 * mult;
	imgNumbers->stretchY = 14 * mult;
	oslDrawImageSimpleXY(imgNumbers, x, y - (((int)digit - digitNb * HAUTEUR_DIGIT) * mult));
	imgNumbers->offsetY0 = (float)(mod(digitNb + 1, 10) * 15);
	imgNumbers->offsetY1 = imgNumbers->offsetY0 + 14.f;
	oslDrawImageSimpleXY(imgNumbers, x, y - (((int)digit - digitNb * HAUTEUR_DIGIT) - HAUTEUR_DIGIT) * mult);
}

void DrawSubMenu(SUBMENU *s, int x, int y, int resetClipping)
{
	int i, yText, posY, nbEle, nbDepart;
	u32 alphaSav, effectSav;

	if (s->child)
		DrawSubMenu(s->child, x, y, resetClipping);
	if (s->active == -1 || s->nbItems <= 0)
		return;

	if (s->type == 2 && resetClipping)
		mySetScreenClipping(x, y, x + LARGEUR_MENU, y + HAUTEUR_TEXTE * MENU_MAX_DISPLAYED_OPTIONS);

	//Offset
	x -= (int)s->xOffset + (int)s->xOffset2;
	if (s->moving)			{
		//Ancienne option
		if (s->active == 2)		{
			x -= s->movingDist * s->moving;
			oslSetAlpha(OSL_FX_ALPHA, (s->movingAlpha * gblMenuAlpha) >> 8);
		}
		else		{
			x -= -s->movingArrive;
			oslSetAlpha(OSL_FX_ALPHA, (max(255 - (s->movingAlpha / 2), 0) * gblMenuAlpha) >> 8);
		}
	}

	oslSetFont(ftGlow);

	//S'il y a un titre
	if (s->type == 1 && s->titre)			{
//		oslSetTextColor(RGB(255,255,255));
//		oslDrawString(x + MENU_LEFT_MARGIN, y, s->titre);
//		oslDrawString(x + MENU_LEFT_MARGIN + 1, y, s->titre);
		MySetTextColorEx(menuColorSubmenuTitle, 0x10);
		oslDrawString(x + MENU_LEFT_MARGIN, y, s->titre);
		MyDrawLine(x + MENU_LEFT_MARGIN, y + 18, x + MENU_LEFT_MARGIN + GetStringWidth(s->titre) + 5, y + 18, RGB(255, 255, 255));
		y += 30;
	}

	//Sauvegarde l'alpha
	alphaSav = osl_currentAlphaCoeff, effectSav = osl_currentAlphaEffect;
	if (effectSav == OSL_FX_DEFAULT)
		alphaSav = 255;

	if (s->type == 3)			{
		SUBMENUMINMAX *mm = (SUBMENUMINMAX*)s->items;
		int xx = x + (170 - LARGEUR_DIGIT * mm->width) / 2, plus;
		char str[100];
		if (resetClipping)
			mySetScreenClipping(x, y + 10, x + LARGEUR_MENU, y + 10 + (HAUTEUR_DIGIT - 2));
		for (i=0;i<mm->width;i++)
			DrawDigitScroll(xx + LARGEUR_DIGIT * i, y + 10, mm->digitsPos[i] + 0.5f);
		oslResetScreenClipping();
		//Dessine les flèches
		oslSetBilinearFilter(1);
		oslSetImageTileSize(imgNumbers, 0, 154, 14, 7);
		if (mm->flecheAnim > 0)		{
			if (mm->flecheAnim < 4)
				plus = mm->flecheAnim;
			else
				plus = 4 - (mm->flecheAnim - 4);
			imgNumbers->stretchX += plus * 2;
			imgNumbers->stretchY += plus;
		}
		else
			plus = 0;
		oslDrawImageSimpleXY(imgNumbers, xx + (int)(mm->flechePosX + 0.5f) - 1 - plus, y - plus);
		oslSetImageTileSize(imgNumbers, 0, 162, 14, 7);
		if (mm->flecheAnim < 0)		{
			if (mm->flecheAnim > -4)
				plus = -mm->flecheAnim;
			else
				plus = 4 - (-mm->flecheAnim - 4);
			imgNumbers->stretchX += plus * 2;
			imgNumbers->stretchY += plus;
		}
		else
			plus = 0;
		oslDrawImageSimpleXY(imgNumbers, xx + (int)(mm->flechePosX + 0.5f) - 1 - plus, y + 27);
		oslSetBilinearFilter(0);
		sprintf(str, "Range: %i - %i", mm->min, mm->max);
		oslDrawString(x + (170 - 5 - GetStringWidth(str)) / 2, y + 50, str);
	}
	else	{
		//Sélection
		posY = (int)(s->state.selPos + 0.5f);
		yText = y;
		if (s->state.active > 0)		{
			//Petit effet spécial pour la sélection
			int val = (menuFrameNb & 127);
			if (menuAnimateCursor)		{
				if (val >= 64)
					val = 64 - (val - 64);
			}
			else
				val = 0;
			val = min(max(s->state.active + (val - 32), 0), 255);
	//		MyDrawGradientRect(x, y + posY, x + LARGEUR_MENU, y + posY + 13, RGBA(255, 255, 128, s->state.active), RGBA(255, 255, 0, s->state.active), RGBA(96, 192, 96, s->state.active), RGBA(0, 192, 0, s->state.active));
			MyDrawGradientRect(x + MENU_LEFT_MARGIN, y + posY, x + MENU_LEFT_MARGIN + LARGEUR_MENU, y + posY + 13,
				menuColorCursor[0] | (val << 24), menuColorCursor[1] | (val << 24),
				menuColorCursor[2] | (val << 24), menuColorCursor[3] | (val << 24));
		}

		oslSetTextColor(RGB(255,255,255));

		nbDepart = ((int)s->state.scrollPos) / HAUTEUR_TEXTE;
		yText -= (int)((s->state.scrollPos - nbDepart * HAUTEUR_TEXTE) + 0.5f);
		nbEle = min(s->nbItems, MENU_MAX_DISPLAYED_OPTIONS) + (((int)s->state.scrollPos + HAUTEUR_TEXTE - 1) / HAUTEUR_TEXTE);

		if (s->state.alphaFleches[0] > 0)		{
			oslSetAlpha(OSL_FX_ALPHA, (s->state.alphaFleches[0] * (alphaSav >> 24)) >> 8);
			oslSetImageTileSize(imgNumbers, 0, 150, 5, 3);
			oslDrawImageSimpleXY(imgNumbers, x, y + 3);
		}
		if (s->state.alphaFleches[1] > 0)		{
			oslSetAlpha(OSL_FX_ALPHA, (s->state.alphaFleches[1] * (alphaSav >> 24)) >> 8);
			oslSetImageTileSize(imgNumbers, 5, 150, 5, 3);
			oslDrawImageSimpleXY(imgNumbers, x, y + MENU_MAX_DISPLAYED_OPTIONS * HAUTEUR_TEXTE - 6);
		}

		oslSetAlpha(effectSav, alphaSav);
		x += MENU_LEFT_MARGIN;
		if (s->type != 2 && resetClipping)
			mySetScreenClipping(x, y, x + LARGEUR_MENU, y + HAUTEUR_TEXTE * MENU_MAX_DISPLAYED_OPTIONS);
//		else
//			mySetScreenClipping(xBase, y, x + LARGEUR_MENU, y + HAUTEUR_TEXTE * MENU_MAX_DISPLAYED_OPTIONS);
		for (i=nbDepart;i<nbEle;i++)		{
			if (s->items[i].disabled & 2)
				MySetTextColor(s->items[i].prop2);
			else if (s->items[i].disabled & 1)
				MySetTextColor(RGB(192, 192, 192));
			else
				MySetTextColor(RGB(255, 255, 255));
			oslDrawString(x + 1, yText, s->items[i].name);
			if (s->items[i].value[0])
				oslDrawString(x + LARGEUR_MENU - GetStringWidth(s->items[i].value) - 2, yText, s->items[i].value);
			yText += HAUTEUR_TEXTE;
		}
		oslSetFont(ftStandard);
	}
	if (resetClipping)
		oslResetScreenClipping();
}

void CloseSubMenu(SUBMENU *s, SUBMENU *next, int moving)
{
	//Ancienne option
	s->active = 2;
	s->moving = moving;
	s->movingDist = 0;
	s->movingAlpha = 255;
	s->movingArrive = 140 * moving;
	//Nouvelle option option
	next->active = 1;
	next->moving = moving;
	next->movingDist = 0;
	next->movingAlpha = 255;
	next->movingArrive = 140 * moving;
}

void HandleSubMenu(SUBMENU *s)		{
	int i;

	if (s->child)
		HandleSubMenu(s->child);

	if (s->active == -1 || s->nbItems <= 0)
		return;

	if (s->active)
		s->state.active = min(s->state.active + 32, STATE_ACTIVE_MAX);
	else
		s->state.active = max(s->state.active - 32, 0);

	if (s->active)			{
		int destPos = 0;
		//Type numérique
		if (s->type == 3)			{
			SUBMENUMINMAX *mm = (SUBMENUMINMAX*)s->items;
			int last = GetIntFromDigits(mm);
			//2 = sous menu
			if (s->active == 1)			{
				if (osl_keys->pressed.right)
					mm->flecheX++;
				if (osl_keys->pressed.left)
					mm->flecheX--;
				mm->flecheX = mod(mm->flecheX, mm->width);

				if (osl_keys->pressed.up)
					mm->digits[mm->flecheX]++, mm->flecheAnim = 1;
				if (osl_keys->pressed.down)
					mm->digits[mm->flecheX]--, mm->flecheAnim = -1;

				for (i=mm->width-1;i>=0;i--)		{
					// < 0 ou > 9
					if (mm->digits[i] < 0)		{
						if (i > 0)		{
							mm->digits[i - 1]--;
							mm->digits[i] += 10;
							mm->digitsPos[i] += 10 * HAUTEUR_DIGIT;
						}
						else
							CreateDigitsFromInt(mm, mm->min, 0);
					}
					if (mm->digits[i] > 9)		{
						if (i > 0)		{
							mm->digits[i - 1]++;
							mm->digits[i] -= 10;
							mm->digitsPos[i] -= 10 * HAUTEUR_DIGIT;
						}
						else
							CreateDigitsFromInt(mm, mm->max, 0);
					}
				}
				i = GetIntFromDigits(mm);
				if (i < mm->min)
					CreateDigitsFromInt(mm, mm->min, 0);
				if (i > mm->max)
					CreateDigitsFromInt(mm, mm->max, 0);
				for (i=mm->width-1;i>=0;i--)		{
					destPos = mm->digits[i] * HAUTEUR_DIGIT;
					if (destPos - mm->digitsPos[i] >= 5 * HAUTEUR_DIGIT)
						mm->digitsPos[i] += 10 * HAUTEUR_DIGIT;
					else if (mm->digitsPos[i] - destPos >= 5 * HAUTEUR_DIGIT)
						mm->digitsPos[i] -= 10 * HAUTEUR_DIGIT;
					mm->digitsPos[i] += (destPos - mm->digitsPos[i]) / 3.0f;
				}
			}
			mm->flechePosX += (mm->flecheX * LARGEUR_DIGIT - mm->flechePosX) / 2.5f;
			//Rien n'a changé? => pas d'emphase sur la flèche
			i = GetIntFromDigits(mm);
			if ((mm->flecheAnim == 1 || mm->flecheAnim == -1) && i == last)
				mm->flecheAnim = 0;
			if (mm->flecheAnim > 0)
				mm->flecheAnim++;
			else if (mm->flecheAnim < 0)
				mm->flecheAnim--;
			if (oslAbs(mm->flecheAnim) > 8)
				mm->flecheAnim = 0;
		}
		else	{
			//Type normal
			if (s->active == 1)			{
				if (osl_keys->pressed.up)		{
					if (osl_keys->held.R)
						s->state.selection = max(s->state.selection - (MENU_MAX_DISPLAYED_OPTIONS-1), 0);
					else
						s->state.selection--;
				}
				if (osl_keys->pressed.down)			{
					if (osl_keys->held.R)
						s->state.selection = min(s->state.selection + (MENU_MAX_DISPLAYED_OPTIONS-1), s->nbItems-1);
					else
						s->state.selection++;
				}
				destPos = 0;
			}
			//Déplacement horizontal quand un choix est sélectionné
			if (gblChoice.parent == s)
				destPos = 10;
			else
				destPos = 0;
			s->xOffset2 += (destPos - s->xOffset2) / 4.0f;

			s->state.selection = mod(s->state.selection, s->nbItems);
			while (s->state.selection + 1 >= s->state.scroll + MENU_MAX_DISPLAYED_OPTIONS
				&& s->state.scroll + MENU_MAX_DISPLAYED_OPTIONS < s->nbItems)
				s->state.scroll++;
			while (s->state.selection - 1 < s->state.scroll
				&& s->state.scroll > 0)
				s->state.scroll--;
			destPos = (s->state.selection - s->state.scroll) * HAUTEUR_TEXTE;
			if (s->state.selectionDirect)		{
				s->state.selPos = (float)destPos;
				s->state.scrollPos = (float)s->state.scroll * HAUTEUR_TEXTE;
				s->state.selectionDirect = 0;
			}
			else	{
				s->state.selPos += (destPos - s->state.selPos) / 2.0f;
				s->state.scrollPos += (s->state.scroll * HAUTEUR_TEXTE - s->state.scrollPos) / 2.0f;
			}

			if (s->state.scroll > 0)
				s->state.alphaFleches[0] = min(s->state.alphaFleches[0] + 32, 255);
			else
				s->state.alphaFleches[0] = max(s->state.alphaFleches[0] - 32, 0);
			if (s->state.scroll + MENU_MAX_DISPLAYED_OPTIONS < s->nbItems)
				s->state.alphaFleches[1] = min(s->state.alphaFleches[1] + 32, 255);
			else
				s->state.alphaFleches[1] = max(s->state.alphaFleches[1] - 32, 0);
		}

		//2 = sous menu
		if (s->active != 2 && s->active != 3)			{
			//Uniquement pour les menus standard
			if (s->type == 1)		{
				if (s->items[s->state.selection].description)
					gblHint.text = s->items[s->state.selection].description, gblHint.active = 1;
				else
					gblHint.active = 0;
			}
			if (osl_keys->pressed.square)			{
				SUBMENU *l = s->items[s->state.selection].link;
				if (l)		{
					if (l->type != 1)			{
						if (l->fctGestion)
							l->fctGestion(l, NULL, EVENT_DEFAULT);
					}
				}
				else		{
					if (s->fctGestion)
						s->fctGestion(s, &s->items[s->state.selection], EVENT_DEFAULT);
				}
			}
			if (osl_keys->pressed.cross && !(s->items[s->state.selection].disabled & 1))		{
				if (s->items[s->state.selection].link)			{
					SUBMENU *link = s->items[s->state.selection].link, *ss;
					//Sous-menu
					if (link->type == 1 || s->type == 2)			{
						int found;
						ss = s;
						found = 0;
						do 			{
							if (link == ss)			{
								found = 1;
								break;
							}
							ss = ss->parent;
						} while (ss);
						if (found)			{
	/*						ss = s;
							do 			{
								ss->child = NULL;
								sss = ss->parent;
								ss->parent = NULL;
								ss = sss;
							} while (ss != link);*/
							CloseSubMenu(s, link, -1);
							s->close = 0;
							link->close = 1;
							SelectSubMenu(link, 0);
						}
						else		{
							//A un enfant
							s->child = link;
							s->child->active = 1;
							s->child->state.active = STATE_ACTIVE_MAX;
							s->child->parent = s;
							s->close = 0;
							s->child->close = 0;

							CloseSubMenu(s, s->child, 1);
							SelectSubMenu(s->child, 1);
						}
					}
					if ((link->type == 2 || link->type == 3) && s->type == 1)		{
						gblChoice.active = 1;
						gblChoice.type = 1;
						gblChoice.submenu = link;
						gblChoice.parent = s;
						s->active = 3;
						s->child = NULL;
						link->active = 1;
						SelectSubMenu(link, 1);
					}
					//Ne plus gérer
					osl_keys->pressed.cross = 0;
				}
				else		{
					//Ne plus gérer, sauf pour les choix
					if (SubMenuItemPositionSelected(s, &s->items[s->state.selection]))		{
						if (s->type == 1)
							osl_keys->pressed.cross = 0;
						else
							CloseChoice(&gblChoice);
					}
				}
			}
			//Annuler
			if (osl_keys->pressed.circle)		{
				if (SubMenuItemPositionCanceled(s, &s->items[s->state.selection]))			{
					if (s->parent)			{
						//Orphelin :(
						CloseSubMenu(s, s->parent, -1);
						SelectSubMenu(s->parent, 0);
						s->parent = NULL;
						s->close = 1;
						//Ne plus gérer
						osl_keys->pressed.circle = 0;
					}
					else
						CloseChoice(&gblChoice);
				}
			}
		}
		//Déplacement
		if (s->moving)		{
			s->movingDist += min(2 + (s->movingDist / 6), 8);
			s->movingArrive -= max(8 + (s->movingDist / 4), 16) * s->moving;
			s->movingAlpha -= 16;
			if ((s->movingArrive < 0 && s->moving == 1) || (s->movingArrive > 0 && s->moving == -1))
				s->movingArrive = 0;
			if (s->movingAlpha < 0)		{
				s->movingAlpha = 0;
				//Ancienne option => désactivée
				if (s->active == 2)			{
					s->active = -1;
					//Remettre si ça bogue
/*					if (s->close)			{
						//Ferme tout
						ss = s;
						do 			{
							sss = ss->parent;
							ss->parent = NULL;
							ss->active = -1;
							ss = sss;
							if (ss)
								ss->child = NULL;
						} while (ss);
						ss = s;
						do 			{
							sss = ss->child;
							ss->child = NULL;
							ss->active = -1;
							ss->parent = NULL;
							ss = sss;
						} while (sss);
					}*/
				}
				else		{
					//Nouvelle option => normale
					s->moving = 0;
/*					if (s->close)			{
						//Ferme tout
						do 			{
							ss = s->child;
							s->child = NULL;
							if (ss)
								ss->parent = NULL;
							else
								break;
							s = ss;
						} while (ss);
					}*/
					//N'a pas d'enfant (nouveau)
					s->child = NULL;
				}
			}
		}
		else
			s->movingDist = 0;
	}
	//Choix => titre en hint
	if (s->type == 2)		{
		if (s->titre)
			gblHint.text = s->titre;
//		else
//			gblHint.active = 0;
	}
}

/*
	Fonctions
*/

void DrawBackground()		{
	//Fond
//	sceGuCopyImage(OSL_PF_5650, 0, 0, 480, 272, 512, imgBack->data, 0, 0, 512, osl_curDrawBuf);
	sceGuDisable(GU_DITHER);
	if (menuConfig.video.background == 1 || !menuIsInGame)		{
		if (imgBack->sizeX != LARG || imgBack->sizeY != HAUT)		{
			imgBack->stretchX = LARG;
			imgBack->stretchY = HAUT;
			oslSetBilinearFilter(1);
		}
		oslDrawImageSimple(imgBack);
		oslSetBilinearFilter(0);
	}
	else	{
		oslClearScreen(0);
		oslSetAlpha(OSL_FX_ALPHA, max(255 - menuFrameNb * 6, 80));
		VideoGuUpdate_Core(menuConfig.video.render, 0);
		oslSetAlpha(OSL_FX_DEFAULT, 0);
	}
	sceGuEnable(GU_DITHER);
//	oslClearScreen(RGB(0,0,128));

	//Icônes standard
	oslSetFont(ftStandard);
	oslSetBkColor(RGBA(0,0,0,0));
	oslSetTextColor(RGB(255,255,255));
	//Haut
	oslSetTextColor(RGB(255,255,255));
	MyDrawFillRect(0, 0, 480, 12, RGBA(0,0,0,128));
	oslDrawString(0, -1, "MasterBoy v2.02.2 by Brunni");
	MyDrawLine(0, 12, 480, 12, RGBA(255, 255, 255, 192));
	//Bas
	oslSetFont(ftStandard);
	oslSetTextColor(RGB(255, 255, 255));
	MyDrawFillRect(0, 260, 480, 272, RGBA(0,0,0,128));
	if (menuStatusBarMessage)
		oslDrawString(0, 259, menuStatusBarMessage);
	MyDrawLine(0, 259, 480, 259, RGBA(255, 255, 255, 192));

	//Batterie
	struct tm *tsys;
	time_t cur_time;
	char menuInfoBattery[128];
	int batteryPercentage = -1;
	//L'heure
	sceKernelLibcTime(&cur_time);
	//Région
	cur_time += 60 * menuTimeZone + 3600 * menuDayLightSaving;
	tsys = localtime(&cur_time);
	//Batterie présente?
	if (scePowerIsBatteryExist()) {
		char batteryTime[128];
		int batteryLifeTime;
		batteryLifeTime = scePowerGetBatteryLifeTime();
		if (scePowerIsPowerOnline())
			strcpy(batteryTime, " (AC)");
		else if (batteryLifeTime >= 0)
			sprintf(batteryTime, " (%ih%02i)", batteryLifeTime / 60, batteryLifeTime % 60);
		else
			batteryTime[0] = 0;
		batteryPercentage = scePowerGetBatteryLifePercent();
/*		static int test = 100;
		if (osl_pad.pressed.circle)
			test++;
		if (osl_pad.pressed.square)
			test--;
		batteryPercentage = test;*/
		sprintf(menuInfoBattery, "%02i:%02i | Batt: %i%%%s, %i°C",
			tsys->tm_hour,tsys->tm_min,
			batteryPercentage,
			batteryTime,
			scePowerGetBatteryTemp());
	/*		sprintf(batteryInfo, "%02d%c%02d Bat.:%s%s%s%02d%%%s Tmp.%dC",
			tsys->tm_hour,(tsys->tm_sec&1?':':' '),tsys->tm_min,
			(scePowerIsPowerOnline()?"Plg.":""),
			(scePowerIsBatteryCharging()?"Chrg.":""),
			(scePowerIsLowBattery()?"Low!":""),
			scePowerGetBatteryLifePercent(),
			bat_time,
			scePowerGetBatteryTemp());*/
	}
	else
		sprintf(menuInfoBattery, "%02i:%02i", tsys->tm_hour, tsys->tm_min);
	if (scePowerIsLowBattery())		{
		oslSetTextColor(RGB(255, 0, 0));
		//Draw a second time for a "bold" effect
		oslDrawString(480 - GetStringWidth(menuInfoBattery) - 23, -1, menuInfoBattery);
	}
	else
		oslSetTextColor(RGB(255, 255, 255));
	oslDrawString(480 - GetStringWidth(menuInfoBattery) - 22, -1, menuInfoBattery);
	oslSetTextColor(RGB(255, 255, 255));

	if (batteryPercentage >= 0 && !(scePowerIsBatteryCharging() && ((menuFrameNb >> 6) & 1)))			{
		int battLevel = (batteryPercentage * 15 + 100 / 2) / 100, i;
		unsigned char multiples[8] = { 0xb0, 0xd0, 0xff, 0xe8, 0xd0, 0xb8, 0xa0, 0x88 };
		OSL_COLOR color = RGB(0, 255, 0), color2;
		//Remplit le dernier bout
		if (battLevel == 15)
			battLevel = 16;
		//Détermine la couleur
		if (scePowerIsPowerOnline() && !scePowerIsBatteryCharging())
			color = RGB(192, 192, 192);
		else		{
			if (batteryPercentage >= 85)		{
				int t = ((batteryPercentage - 85) * 255) / 15;
				color = RGB(0, 255 - t / 2, 255);
			}
			else if (batteryPercentage >= 70)		{
				int t = ((batteryPercentage - 70) * 255) / 15;
				color = RGB(0, 192 + t / 4, t);
			}
			else if (batteryPercentage >= 55)		{
				int t = ((batteryPercentage - 55) * 255) / 15;
				color = RGB(255 - t, 255 - t / 4, 0);
//				color = RGB(255 - t, 255 - t / 2, t);
			}
			else if (batteryPercentage >= 15)		{
				int t = ((batteryPercentage - 15) * 255) / 40;
				color = RGB(255, t, 0);
			}
			else
				color = RGB(255, 0, 0);
		}
		for (i=0;i<8;i++)		{
			color2 = ((color & 0xff) * multiples[i]) >> 8;
			color2 |= (((color & 0xff00) * multiples[i]) >> 8) & 0xff00;
			color2 |= (((color & 0xff0000) * multiples[i]) >> 8) & 0xff0000;
			color2 |= color & 0xff000000;
			oslDrawLine(479 - battLevel, i + 2, 479, i + 3, color2);
		}
		oslSetImageTileSize(imgNumbers, 0, 169, 18, 10);
		oslDrawImageXY(imgNumbers, 480 - 18, 1);
	}
}

void DrawBackgroundAfter()		{
	DrawWindow(menuCurrentWindow);
	//Fond
	if (fadeLevel > 0)		{
		mySetScreenClipping(0, 13, 480, 259);
//		oslSetImageTile(imgBack, 0, 13, 480, 259);
//		imgBack->y = 13;
		sceGuDisable(GU_DITHER);
		if (menuConfig.video.background == 1 || !menuIsInGame)			{
			oslSetAlpha(OSL_FX_ALPHA, fadeLevel);
			if (imgBack->sizeX != LARG || imgBack->sizeY != HAUT)
				oslSetBilinearFilter(1);
			oslDrawImageSimple(imgBack);
		}
		else			{
			int v = max(255 - menuFrameNb * 6, 80);
			oslSetAlpha(OSL_FX_ALPHA|OSL_FX_COLOR, RGBA(v, v, v, fadeLevel));
			VideoGuUpdate_Core(menuConfig.video.render, 0);
		}
		sceGuEnable(GU_DITHER);
//		oslResetImageTile(imgBack);
//		imgBack->y = 0;
		oslResetScreenClipping();
	}
	oslSetAlpha(OSL_FX_DEFAULT, 0);
	menuSoundShowDisplay();

#ifdef DEBUG_MODE
	if (menuDebugRamShow)		{
		oslSetFont(ftGlow);
		oslSetTextColor(RGB(255, 255, 255));
		oslPrintf_xy(0, 20, "RAM: %ik", ramAvailable() >> 10);
	}
#endif
//	oslSysBenchmarkDisplay();
}

void HandleBackground()		{
	//La fenêtre courante
	HandleWindow(menuCurrentWindow);

	//Fade
	fadeLevel += fadeDirection;
	if (fadeLevel >= 256 && fadeDirection > 0)			{
		//Action
		fadeDirection = fadeLevel = 0;
		//Load ROM
		if (fadeReason == 1)		{
			stExtentions = stRomExtentions;
			ShowMenuFileSelect(gblRomPath, 0);
			//Fichier sélectionné?
			if (menuFileSelectFileName[0])		{
				char temp[MAX_PATH];
				int lastMachine = gblMachineType;
				MessageBoxEx(menuFileSelectFileName, "Now loading...", MB_NONE, 300);
				MenuPlusAction(MA_LOADROM, menuFileSelectFileName);
				CloseWindow(menuCurrentWindow, 0);
				if (gblFlagLoadParams)		{
					//Try to open the machine specific config (first time only or if machine changes)
					if (gblMachineType != lastMachine || !menuIsInGame)
						LoadDefaultMachineConfig();
					//Try to open the game specific config
					pspGetStateNameEx(menuFileSelectFileName, temp, STATE_CONFIG);
					OuvreFichierConfig(temp, NULL);
				}
				if (gblMachineType == EM_GBC)			{
					gb_reset();
					machine_manage_sram(SRAM_LOAD, 0);
				}
			}
			fadeInit(-24, 0);
		}
		else if (fadeReason == 2)		{
			ShowMenuJoystickCalibration();
			fadeInit(-24, 0);
		}
		//Load Music
		else if (fadeReason == 3)		{
			stExtentions = stMusicExtentions;
			ShowMenuFileSelect(gblMusicPath, 1);
			//Fichier sélectionné?
			if (menuFileSelectFileName[0])		{
				strcpy(menuConfig.music.filename, menuFileSelectFileName);
				MessageBoxEx(menuFileSelectFileName, "Now loading...", MB_NONE, 300);
				menuStartSound(menuFileSelectFileName);
				CloseWindow(menuCurrentWindow, 0);
			}
			fadeInit(-24, 0);
		}
	}
	if (fadeLevel < 0 && fadeDirection < 0)			{
		//Terminé
		fadeDirection = fadeLevel = 0;
	}
	menuFrameNb++;
}

void menuLoadFilesMin()			{
	if (!ftStandard)		{
		ftStandard = oslLoadFontFile("res/font.oft");
		ftGlow = oslLoadFontFile("res/glow.oft");
		CreeImageBord();
	}
	//Initialize the gzio file system
	vfsGzFileInit();
	oslAssert(ftStandard && ftGlow && imgBord);
}

void menuLoadFiles()
{
	menuLoadFilesMin();
	if (!imgIcons)			{
		OSL_IMAGE *temp;
		imgBack = loadBackgroundImage("res/back.png");
		oslSetTransparentColor(RGB(255, 0, 254));
		imgIcons = oslLoadImageFile("res/icons.png", OSL_IN_RAM, OSL_PF_5551);
		imgNumbers = oslLoadImageFile("res/numbers.png", OSL_IN_RAM, OSL_PF_5551);
		oslDisableTransparentColor();
	}
	oslAssert(imgBack && imgIcons && imgNumbers);
}

void menuDrawIcons(MENU *m, int angle, int ajoutRayon)
{
	int i;
	float scale, x, y, stretchX, stretchY;
	int angleDiff = 360 / m->nbItems;

	angle -= 90;
	for (i=0;i<m->nbItems;i++)		{
		y = 40.f - oslSin(angle, 20 + ajoutRayon);
		x = 240.f - 4 + oslCos(angle, 120 + ajoutRayon);
		scale = (y) / 60.0f;
		oslSetImageTileSize(imgIcons, 0, m->items[i].iconOffsetY, 32, 32);
		stretchX = 32.f * scale;
		stretchY = 32.f * scale;
		//Bilinéaire si redimensionnement
		oslSetBilinearFilter((int)(stretchX+0.5f) != 32 || (int)(stretchY+0.5f) != 32);
		menuDrawImageSmallFloat(imgIcons, x, y, stretchX, stretchY);
		angle = (angle + angleDiff) % 360;
//		ajoutRayon -= 4;
//		if (ajoutRayon < 0)	ajoutRayon = 0;
	}
	oslSetBilinearFilter(0);
}

/*void menuDrawMenu(MENUITEM *mi, int xOffset)		{
	MENURETURN *mr;
	int xBase = 100 - xOffset, yBase = 100;
	oslSetTextColor(RGB(255,128,0));
	oslDrawString(xBase, yBase, mi->name);
	oslDrawString(xBase + 1, yBase, mi->name);
	MyDrawLine(xBase, yBase + 18, xBase + GetStringWidth(mi->name) + 5, yBase + 18, RGB(255, 255, 0));
//	mr = mi->fonction(mi, ACTION_DRAW, xBase, yBase + 30);
	DrawSubMenu(mi->subMenu, xBase, yBase + 30);
}*/

void InitHint(HINT *h)
{
	h->active = 0;
	h->position = 0.f;
}

void HandleHint(HINT *h)
{
	int dest;
	dest = h->active ? 43 : 0;
	h->position += (dest - h->position) / 2.5f;
}

void DrawHint(HINT *h)
{
	//Dessin
	if (h->position > 0 && h->text)			{
		int position = (int)(h->position + 0.5f);
		mySetScreenClipping(260, 259 - position, 480, 259);
		MyDrawLine(261, 259 - position, 480, 259 - position, RGBA(255, 255, 255, 128));
		MyDrawLine(260, 259 - position, 260, 259, RGBA(255, 255, 255, 128));
		MyDrawFillRect(261, 260 - position, 480, 259, RGBA(0, 0, 0, 128));
		oslDrawTextBox(263, 261 - position, 480, 261 - position + 41, h->text, 0);
		oslResetScreenClipping();
	}
}

void InitChoice(CHOICEMENU *h)
{
	h->active = 0;
	h->position = 0.f;
	h->submenu = NULL;
}

void CloseChoice(CHOICEMENU *h)		{
	h->active = 0;
	if (h->parent)
		h->parent->active = 1;
	h->parent = NULL;
	if (h->submenu)
		h->submenu->active = 2;
}

void HandleChoice(CHOICEMENU *h)
{
	int dest;
	dest = h->active ? CHOICE_MAX_POS : 0;
	h->position += (dest - h->position) / 2.5f;
	if (h->position >= 1.0f && h->active)			{
		HandleSubMenu(h->submenu);
		//Fermeture
//		if (osl_keys->pressed.circle || osl_keys->pressed.cross)
//			CloseChoice(h);
	}
}

void DrawChoice(CHOICEMENU *h)
{
	//Dessin
	if (h->position >= 1.0f)			{
		int positionY = (int)(gblHint.position + 0.5f);
		int positionX = (int)(h->position + 0.5f);
//		mySetScreenClipping(260, 259 - position, 480, 259);
//		MyDrawLine(261, 259 - position, 480, 259 - position, RGBA(255, 0, 0, 192));
//		MyDrawLine(260, 259 - position, 260, 259, RGBA(255, 0, 0, 192));
		#define alpha 192
//			MyDrawGradientRect(480 - positionX, 13, 480, 259 - positionY, RGBA(128, 128, 128, alpha), RGBA(64, 192, 192, alpha), RGBA(128, 128, 128, alpha), RGBA(64, 160, 192, alpha));
			MyDrawGradientRect(480 - positionX, 13, 480 + CHOICE_MAX_POS - positionX, 259 - positionY, RGBA(64, 64, 64, alpha), RGBA(48, 72, 112, alpha), RGBA(64, 64, 64, alpha), RGBA(48, 72, 112, alpha));
		#undef alpha
		DrawSubMenu(h->submenu, 480 - positionX + 10 - MENU_LEFT_MARGIN, 100, 1);
//		oslDrawTextBox(263, 261 - position, 480, 259, h->text, 0);
//		oslResetScreenClipping();
	}
}

void InitConfig()
{
	int i;

	menuConfigDefault = malloc(3 * sizeof(MENUPARAMS));
	menuConfigUserDefault = menuConfigDefault + 1;
	menuConfigUserMachineDefault = menuConfigDefault + 2;

	menuConfig.file.state = 0;
	menuConfig.file.wifiEnabled = 0;
	menuConfig.file.sramAutosave = 1;
	strcpy(gblRomPath, "ms0:/PSP/GAME/");

	menuConfig.music.frequency = 44100;
	menuConfig.music.repeatMode = 0;
	strcpy(gblMusicPath, "ms0:/PSP/GAME/");
	menuConfig.music.filename[0] = 0;

	menuConfig.video.country = 0;
	menuConfig.video.render = 3;
	menuConfig.video.smoothing = 1;
	menuConfig.video.frameskip = 1;
	menuConfig.video.frameskipMax = 4;
	menuConfig.video.vsync = 0;
	menuConfig.video.background = 1;
	menuConfig.video.gamma = 100;
	menuConfig.video.vibrance = 128;
	menuConfig.video.cpuTime = 0;
	menuConfig.video.turboSkip = 2;
	menuConfig.video.renderDepth = 8;
	//Auto
	menuConfig.video.leftBar = 2;
	//No sprite limit by default
	menuConfig.video.spriteLimit = 0;
	//Loose sync mode by default
	menuConfig.video.syncMode = 1;
	menuConfig.gameboy.colorization = 1;
	menuConfig.gameboy.palette[0] = 0;
	menuConfig.gameboy.gbType = 0;

	menuConfig.sound.enabled = 1;
	menuConfig.sound.sampleRate = 44100;
	menuConfig.sound.stereo = 0;
	menuConfig.sound.volume = 0;
	menuConfig.sound.turboSound = 2;
	menuConfig.sound.perfectSynchro = 1;

	menuConfig.ctrl.analog.useCalibration = 0;
	menuConfig.ctrl.analog.toPad = 1;
	menuConfig.ctrl.analog.treshold = 80;
	menuConfig.ctrl.keys.up = OSL_KEYMASK_UP;
	menuConfig.ctrl.keys.down = OSL_KEYMASK_DOWN;
	menuConfig.ctrl.keys.left = OSL_KEYMASK_LEFT;
	menuConfig.ctrl.keys.right = OSL_KEYMASK_RIGHT;
	menuConfig.ctrl.keys.button1 = OSL_KEYMASK_CROSS;
	menuConfig.ctrl.keys.button2 = OSL_KEYMASK_CIRCLE;
	menuConfig.ctrl.keys.start = OSL_KEYMASK_START;
	menuConfig.ctrl.keys.select = OSL_KEYMASK_SELECT;
	menuConfig.ctrl.keys.auto1 = 0;
	menuConfig.ctrl.keys.auto2 = 0;
	menuConfig.ctrl.cuts.menu = OSL_KEYMASK_L;
	menuConfig.ctrl.cuts.pause = OSL_KEYMASK_R | OSL_KEYMASK_CROSS;
	menuConfig.ctrl.cuts.turbo = OSL_KEYMASK_R | OSL_KEYMASK_SQUARE;
	menuConfig.ctrl.cuts.sload = OSL_KEYMASK_R | OSL_KEYMASK_START;
	menuConfig.ctrl.cuts.ssave = OSL_KEYMASK_R | OSL_KEYMASK_SELECT;
	menuConfig.ctrl.cuts.splus = OSL_KEYMASK_R | OSL_KEYMASK_UP;
	menuConfig.ctrl.cuts.sminus = OSL_KEYMASK_R | OSL_KEYMASK_DOWN;
	menuConfig.ctrl.cuts.reset = 0;
	menuConfig.ctrl.cuts.musicplayer = OSL_KEYMASK_SELECT;
	for (i=0;i<4;i++)
		menuConfig.ctrl.analog.calValues[i] = 0;
	menuConfig.ctrl.autofireRate = 2;

	menuConfig.misc.pspClock = 222;
	menuConfig.misc.z80Clock = 100;
	menuConfig.misc.SMSLines = 0;

	//Touches
	menuPressedKeysAutorepeatInit = 30;
	menuPressedKeysAutorepeatDelay = 8;
	menuPressedKeysAutorepeatMask = MENUKEY_AUTOREPEAT_MASK;
/*	menuColorCursor[0] = RGBA(144, 144, 144, 0);
	menuColorCursor[1] = RGBA(160, 160, 144, 0);
	menuColorCursor[2] = RGBA(80, 80, 80, 0);
	menuColorCursor[3] = RGBA(96, 96, 96, 0);
	menuColorSubmenuTitle = RGB(255, 255, 0);
	menuAnimateCursor = 1;*/
	int valeur = RGB(192, 0, 0);
	menuColorCursor[0] = GetAddedColor(valeur, 16);
	menuColorCursor[1] = GetAddedColor(valeur, 32);
	menuColorCursor[2] = GetAddedColor(valeur, -48);
	menuColorCursor[3] = GetAddedColor(valeur, -32);
	menuColorSubmenuTitle = RGB(255, 255, 255);
	menuAnimateCursor = 1;
	menuStatusMessage[0] = 0;


	//Initialisation du save state
	if (!menuRamStateMemory)		{
		menuRamStateMemory = malloc(RAM_STATE_SIZE);
		menuRamStateFileName[0] = 0;
	}

	//Système
	char sValue[256];
	int iValue;
	
	menuTimeZone=0;
	menuDayLightSaving=0;

	if (sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_TIMEZONE, &iValue) != PSP_SYSTEMPARAM_RETVAL_FAIL)
		menuTimeZone = iValue;
	if (sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_DAYLIGHTSAVINGS, &iValue) != PSP_SYSTEMPARAM_RETVAL_FAIL)
		menuDayLightSaving = iValue;
/*	if (sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &iValue)!=PSP_SYSTEMPARAM_RETVAL_FAIL)
		menuLanguage = iValue;					//Défaut: PSP_SYSTEMPARAM_LANGUAGE_ENGLISH
	if (sceUtilityGetSystemParamString(PSP_SYSTEMPARAM_ID_STRING_NICKNAME, sValue, 256) != PSP_SYSTEMPARAM_RETVAL_FAIL)
	{
		//get nick name		
		//now convert to sjis
		int i=0, j=0, k;
		unsigned int utf8;
		while (sValue[i]) {
			utf8 = (uint8)sValue[i++];
			utf8 = (utf8 << 8) | (uint8)sValue[i++];
			utf8 = (utf8 << 8) | (uint8)sValue[i++];
			
			for (k=0;k<sjis_xlate_entries;k++)		{
				if (utf8 == sjis_xlate[k].utf8)			{
					menuNickname[j++] = sjis_xlate[k].sjis >> 8;
					menuNickname[j++] = sjis_xlate[k].sjis & 0xFF;
					break;
				}
			}
		}
		menuNickname[j]=0;				
	}*/
}

int SetSubMenuItemValueByInt(SUBMENUITEM *s, int value)		{
	SUBMENU *menu = s->link;
	int i, j, success = 0;
	if (menu)		{
		for (i=0;i<menu->nbItems;i++)		{
			if (menu->items[i].prop1 == value)		{
				strcpy(s->value, menu->items[i].name);
				success = 1;
				break;
			}
			if (menu->items[i].prop1 == MASK_DEFAULT_MENU_VALUE)
				strcpy(s->value, menu->items[i].name);
		}
	}
	return success;
}

int RangeSetSubMenuValueInt(SUBMENUITEM *s, int *value, int set)		{
	SUBMENU *menu = s->link;
	int i, j, success = 0;
	if (menu)		{
		if (menu->type == 3)			{
			SUBMENUMINMAX *smm = (SUBMENUMINMAX*)menu->items;
			if (*value < smm->min)
				*value = smm->min;
			if (*value > smm->max)
				*value = smm->max;
			success = 1;
		}
		else	{
			for (i=0;i<menu->nbItems;i++)		{
				if (menu->items[i].prop1 == *value)		{
					success = 1;
					break;
				}
				if (menu->items[i].prop1 == MASK_DEFAULT_MENU_VALUE)			{
					SUBMENU *l = menu->items[i].link;
					if (l)		{
						//Numérique?
						if (l->type == 3)		{
							SUBMENUMINMAX *smm = (SUBMENUMINMAX*)l->items;
							if (*value < smm->min)
								*value = smm->min;
							if (*value > smm->max)
								*value = smm->max;
							success = 1;
							break;
						}
					}
				}
			}
		}
	}
	if (!success)
		*value = menu->items[0].prop1;
	if (set)
		SetSubMenuItemValueByInt(s, *value);
	return success;
}

void UpdateMenus(int elements)
{
	if (elements & MENUUPD_FILE)		{
		RangeSetSubMenuValueInt(&menuMainFileSramItems[0], &menuConfig.file.sramAutosave, 1);
		RangeSetSubMenuValueInt(&menuMainFileMusicItems[1], &menuConfig.music.frequency, 0);
		sprintf(menuMainFileMusicItems[1].value, "%i Hz", menuConfig.music.frequency);
		RangeSetSubMenuValueInt(&menuMainFileMusicItems[2], &menuConfig.music.repeatMode, 1);
	}
	if (elements & MENUUPD_VIDEO)		{
		RangeSetSubMenuValueInt(&menuMainVideoRenderItems[0], &menuConfig.video.render, 1);
		RangeSetSubMenuValueInt(&menuMainVideoRenderItems[1], &menuConfig.video.smoothing, 1);
		RangeSetSubMenuValueInt(&menuMainVideoRenderItems[2], &menuConfig.video.renderDepth, 1);
		RangeSetSubMenuValueInt(&menuMainVideoRenderItems[3], &menuConfig.video.brightnessMode, 1);
		RangeSetSubMenuValueInt(&menuMainVideoRenderItems[4], &menuConfig.video.gamma, 0);
		sprintf(menuMainVideoRenderItems[4].value, "%i.%02i", menuConfig.video.gamma / 100, menuConfig.video.gamma % 100);
		//Gamma inactif si le mode n'est pas gamma
//		menuMainVideoRenderItems[4].disabled = (menuConfig.video.brightnessMode == 1);

		sprintf(menuMainVideoRenderItems[5].value, "%i", menuConfig.video.vibrance);
		RangeSetSubMenuValueInt(&menuMainVideoRenderItems[5], &menuConfig.video.vibrance, 0);
		//Frameskip max toujours supérieur au frameskip
		menuConfig.video.frameskipMax = max(menuConfig.video.frameskip, menuConfig.video.frameskipMax);
		RangeSetSubMenuValueInt(&menuMainVideoRenderItems[6], &menuConfig.video.leftBar, 1);
		RangeSetSubMenuValueInt(&menuMainVideoRenderItems[7], &menuConfig.video.spriteLimit, 1);
		RangeSetSubMenuValueInt(&menuMainVideoSyncItems[0], &menuConfig.video.frameskip, 1);
		RangeSetSubMenuValueInt(&menuMainVideoSyncItems[1], &menuConfig.video.frameskipMax, 1);
		RangeSetSubMenuValueInt(&menuMainVideoSyncItems[2], &menuConfig.video.vsync, 1);
		RangeSetSubMenuValueInt(&menuMainVideoSyncItems[3], &menuConfig.video.syncMode, 1);

		RangeSetSubMenuValueInt(&menuMainVideoMiscItems[0], &menuConfig.video.country, 1);
		RangeSetSubMenuValueInt(&menuMainVideoMiscItems[1], &menuConfig.video.turboSkip, 0);
		sprintf(menuMainVideoMiscItems[1].value, "%i frame%c", menuConfig.video.turboSkip, menuConfig.video.turboSkip > 1 ? 's' : 0);
		RangeSetSubMenuValueInt(&menuMainVideoMiscItems[2], &menuConfig.video.cpuTime, 1);

		//Game Boy
		strcpy(menuMainVideoGBItems[0].value, menuConfig.gameboy.palette[0] ? menuConfig.gameboy.palette : "[Default]");
		RangeSetSubMenuValueInt(&menuMainVideoGBItems[1], &menuConfig.gameboy.gbType, 1);
		RangeSetSubMenuValueInt(&menuMainVideoGBItems[2], &menuConfig.gameboy.colorization, 1);
	}
	if (elements & MENUUPD_SOUND)		{
		RangeSetSubMenuValueInt(&menuMainSoundItems[0], &menuConfig.sound.enabled, 1);
		RangeSetSubMenuValueInt(&menuMainSoundItems[1], &menuConfig.sound.sampleRate, 1);
		RangeSetSubMenuValueInt(&menuMainSoundItems[2], &menuConfig.sound.stereo, 1);
		RangeSetSubMenuValueInt(&menuMainSoundItems[3], &menuConfig.sound.volume, 1);
		RangeSetSubMenuValueInt(&menuMainSoundItems[4], &menuConfig.sound.perfectSynchro, 1);
		RangeSetSubMenuValueInt(&menuMainSoundItems[5], &menuConfig.sound.turboSound, 1);
	}
	if (elements & MENUUPD_CTRL)		{
		RangeSetSubMenuValueInt(&menuMainCtrlAnalogCalibrationItems[0], &menuConfig.ctrl.analog.toPad, 1);
		RangeSetSubMenuValueInt(&menuMainCtrlAnalogCalibrationItems[1], &menuConfig.ctrl.analog.treshold, 0);
		sprintf(menuMainCtrlAnalogCalibrationItems[1].value, "%i", menuConfig.ctrl.analog.treshold);
		RangeSetSubMenuValueInt(&menuMainCtrlAnalogCalibrationItems[2], &menuConfig.ctrl.analog.useCalibration, 1);
		RangeSetSubMenuValueInt(GetSubMenuItemByInt(&menuMainCtrlRedefine, 88), &menuConfig.ctrl.autofireRate, 0);
		sprintf(GetSubMenuItemByInt(&menuMainCtrlRedefine, 88)->value, "%i frames", menuConfig.ctrl.autofireRate);
	}
	if (elements & MENUUPD_MISC)		{
		RangeSetSubMenuValueInt(&menuMainMiscItems[0], &menuConfig.misc.pspClock, 0);
		RangeSetSubMenuValueInt(&menuMainMiscItems[1], &menuConfig.misc.z80Clock, 0);
		sprintf(menuMainMiscItems[0].value, "%i MHz", menuConfig.misc.pspClock);
		sprintf(menuMainMiscItems[1].value, "%i%%", menuConfig.misc.z80Clock);
		RangeSetSubMenuValueInt(&menuMainMiscUIItems[0], &menuConfig.video.background, 1);
		if (menuConfig.misc.pspClock >= 100 && menuConfig.misc.pspClock <= 333)
			scePowerSetClockFrequency (menuConfig.misc.pspClock, menuConfig.misc.pspClock, menuConfig.misc.pspClock / 2);
	}
}

u32 __attribute__((aligned(16))) windowBoxPalette[3][16];

void DrawWindowBox(int x0, int y0, int x1, int y1)
{
	const int alphaBoite = 192, couleurTitre1 = RGBA(128, 0, 0, alphaBoite), couleurTitre2 = RGBA(128, 128, 128, alphaBoite),
		couleurBody1 = RGBA(0, 0, 192, alphaBoite), couleurBody2 = RGBA(128, 0, 192, alphaBoite),
		couleurContour = RGB(0, 192, 128), couleurContourMilieu = couleurContour;
	void *oldPal = imgBord->palette->data;
	int i, j;

	windowBoxPalette[0][0] = 0;
	windowBoxPalette[0][1] = couleurContour;
	windowBoxPalette[0][2] = couleurTitre1;
	windowBoxPalette[1][0] = 0;
	windowBoxPalette[1][1] = couleurContour;
	windowBoxPalette[1][2] = couleurTitre2;
	windowBoxPalette[2][0] = 0;
	windowBoxPalette[2][1] = couleurContour;
	windowBoxPalette[2][2] = couleurBody2;

	imgBord->palette->data = windowBoxPalette[0];
	oslUncachePalette(imgBord->palette);
		oslDrawImageSimpleXY(imgBord, x0 + 1, y0 + 1);
		oslMirrorImageH(imgBord);
	imgBord->palette->data = windowBoxPalette[1];
	oslUncachePalette(imgBord->palette);
		oslDrawImageSimpleXY(imgBord, x1 - 6, y0 + 1);
		oslMirrorImageV(imgBord);
	imgBord->palette->data = windowBoxPalette[2];
	oslUncachePalette(imgBord->palette);
		oslDrawImageSimpleXY(imgBord, x1 - 6, y1 - 6);
		oslMirrorImageH(imgBord);
		oslDrawImageSimpleXY(imgBord, x0 + 1, y1 - 6);
		oslMirrorImageV(imgBord);
	imgBord->palette->data = oldPal;

	MyDrawLine(x0 + 6, y0, x1 - 6, y0, couleurContour);
	MyDrawLine(x0, y0 + 6, x0, y1 - 6, couleurContour);
	MyDrawLine(x0 + 6, y1 - 1, x1 - 6, y1 - 1, couleurContour);
	MyDrawLine(x1 - 1, y0 + 6, x1 - 1, y1 - 6, couleurContour);

	MyDrawLine(x0, y0 + 14, x1, y0 + 14, couleurContourMilieu);
//	MyDrawFillRect(x0 + 6, y0 + 1, x1 - 6, y0 + 6, couleurTitre);
//	MyDrawFillRect(x0 + 1, y0 + 6, x1 - 1, y0 + 14, couleurTitre);
	MyDrawGradientRect(x0 + 6, y0 + 1, x1 - 6, y0 + 14, couleurTitre1, couleurTitre2, couleurTitre1, couleurTitre2);
	MyDrawFillRect(x0 + 1, y0 + 6, x0 + 6, y0 + 14, couleurTitre1);
	MyDrawFillRect(x1 - 6, y0 + 6, x1 - 1, y0 + 14, couleurTitre2);

//	MyDrawFillRect(x0 + 1, y0 + 15, x1 - 1, y1 - 6, couleurBody);
	MyDrawGradientRect(x0 + 1, y0 + 15, x1 - 1, y1 - 6, couleurBody1, couleurBody1, couleurBody2, couleurBody2);
	MyDrawFillRect(x0 + 6, y1 - 6, x1 - 6, y1 - 1, couleurBody2);
}

void DrawWindow(WINDOW *w)		{
	int x0, y0, x1, y1, larg, haut, xx0, yy0, xx1, yy1, x2, y2, x3;
	OSL_FONT *ft;

	if (!w)
		return;
	if (w->state == -1 || w->multiply <= 0)
		return;

	//S'assure que les fichiers vitaux sont chargés
	if (!imgBord)
		menuLoadFilesMin();

	ft = osl_curFont;
	oslSetFont(ftGlow);
	larg = (w->largeur * w->multiply) >> 8;
	haut = (w->hauteur * w->multiply) >> 8;
	x0 = (LARG - larg) / 2;
	x1 = x0 + larg;
	y0 = (HAUT - haut) / 2;
	y1 = y0 + haut;

	xx0 = (LARG - w->largeur) / 2;
	xx1 = xx0 + w->largeur;
	yy0 = (HAUT - w->hauteur) / 2;
	yy1 = yy0 + w->hauteur;

	mySetScreenClipping(x0, y0, x1, y1);
	DrawWindowBox(x0, y0, x1, y1);
//	oslSetBkColor(RGBA(0, 0, 0, 0));
	MySetTextColor(RGB(255, 255, 255));
	//8 pixels perdus
	oslDrawTextBox(xx0 + 4, yy0 + 18, xx1 - 4, yy1 - 6, w->texte, 0);
	oslDrawString((xx1 + xx0) / 2 - GetStringWidth(w->titre) / 2, yy0 + 1, w->titre);

	if (w->buttons == MB_OKCANCEL)			{
		x2 = (5 * xx1 + 11 * xx0) / 16 - GetStringWidth(w->button1) / 2;
		x3 = (11 * xx1 + 5 * xx0) / 16 - GetStringWidth(w->button2) / 2;
	}
	else if (w->buttons == MB_OK || w->buttons == MB_CANCEL)		{
		x2 = (xx1 + xx0) / 2 - GetStringWidth(w->button1) / 2;
		x3 = x2;
	}
	y2 = yy1 - 16;

	oslSetFont(ftStandard);
	oslSetBkColor(RGBA(0, 0, 0, 0));
	oslSetTextColor(RGB(0, 0, 0));
	if (w->buttons & MB_OK)
		oslDrawString(x2 + 1, y2 + 1, w->button1);
	if (w->buttons & MB_CANCEL)
		oslDrawString(x3 + 1, y2 + 1, w->button2);
	oslSetTextColor(RGB(0, 255, 0));
	if (w->buttons & MB_OK)
		oslDrawString(x2, y2, w->button1);
	oslSetTextColor(RGB(255, 0, 0));
	if (w->buttons & MB_CANCEL)
		oslDrawString(x3, y2, w->button2);
	if (w->fctHandle)
		w->fctHandle(w, EVENT_DRAW);
	oslResetScreenClipping();
	oslSetFont(ft);
}

void CloseWindow(WINDOW *w, int valid)		{
	if (!w)
		return;
	if (w->state && w->fctClose)
		w->fctClose(w, valid);
	w->state = 0;
	w->selected = valid;
}

void DestroyWindow(WINDOW *w)		{
	if (w->state == 1)
		CloseWindow(w, 0);
	if (w->fctDestroy)
		w->fctDestroy(w);
	w->state = -1;
	menuCurrentWindow = NULL;
}

void HandleWindow(WINDOW *w)		{
	if (!w)
		return;
	if (w->state == -1)
		return;

	if (w->state == 0)
		w->multiply = max(w->multiply - 48, 0);
	else if (w->state == 1)
		w->multiply = min(w->multiply + 48, MSGBOX_MAX_MULTIPLY);

	if (w->fctHandle)
		w->fctHandle(w, EVENT_HANDLE);

	if (w->state == 1 && w->buttons != MB_NONE)		{
		if (osl_keys->pressed.cross)
			CloseWindow(w, 1);
		if (osl_keys->pressed.circle)
			CloseWindow(w, 0);
		//Les autres ne doivent pas gérer de touche dans ce mode!
		osl_keys->pressed.value = 0;
	}
	//Fermer la fenêtre
	if (w->state == 0 && w->multiply == 0)
		DestroyWindow(w);
}

void ShowWindow(WINDOW *w)		{
	if (menuCurrentWindow)		{
		DestroyWindow(w);
	}
	menuCurrentWindow = w;
	w->state = 1;
	if (w->hauteur == -1)
		//8 pixels perdus
		w->hauteur = GetTextBoxHeight(0, w->largeur - 8, w->texte) + 24 + (w->buttons? 16 : 0);
	if (w->fctHandle)
		w->fctHandle(w, EVENT_INIT);
	//fctClose -> mode asynchrone
	if (!w->fctClose)		{
		OSL_IMAGE *imgFond;
		int skip = 0;
		//Copie le backbuffer sur une image
		oslLockImage(OSL_SECONDARY_BUFFER);
			imgFond = oslCreateSwizzledImage(OSL_SECONDARY_BUFFER, OSL_IN_RAM);
//			imgFond = oslCreateImageCopy(OSL_DEFAULT_BUFFER, OSL_IN_RAM);
		oslUnlockImage(OSL_SECONDARY_BUFFER);
		if (menuIsInGame == 2)
			SoundPause();
		while(!osl_quit && w->state == 1)			{
			MyReadKeys();
			menuStandardVblank();
			//Pas de bouton => direct au max
			if (w->buttons == MB_NONE)
				w->multiply = MSGBOX_MAX_MULTIPLY;
			HandleWindow(w);
			//Dessine une fois pour MB_NONE
			if (!skip || w->buttons == MB_NONE)		{
				oslStartDrawing();
				sceGuDisable(GU_DITHER);
				oslDrawImage(imgFond);
				sceGuEnable(GU_DITHER);
				DrawWindow(w);
				oslEndDrawing();
			}
			skip = oslSyncFrame();
			if (w->buttons == MB_NONE)
				break;
		}
		oslDeleteImage(imgFond);
		if (menuIsInGame == 2)
			SoundResume();
	}
}

int MessageBox_DefineButtons(WINDOW *w, int *type)		{
	if (*type == MB_YESNO)		{
		w->button1 = "X Yes";
		w->button2 = "O No";
		*type = MB_OKCANCEL;
	}
	else		{
		w->button1 = "X Ok";
		w->button2 = "O Cancel";
	}
}

int MessageBoxEx(const char *texte, const char *titre, int type, int width)		{
	memset(&winMsgBox, 0, sizeof(winMsgBox));
	MessageBox_DefineButtons(&winMsgBox, &type);
	winMsgBox.hauteur = -1;
	winMsgBox.largeur = width;
	winMsgBox.texte = texte;
	winMsgBox.titre = titre;
	winMsgBox.fctClose = NULL;
	winMsgBox.buttons = type;
	winMsgBox.fctDestroy = NULL;
	ShowWindow(&winMsgBox);
	return winMsgBox.selected;
}

int MessageBoxAsync(const char *texte, const char *titre, int type, int width, void (*fctClose)(struct WINDOW*, int), void (*fctHandle)(struct WINDOW*, int))		{
	memset(&winMsgBox, 0, sizeof(winMsgBox));
	MessageBox_DefineButtons(&winMsgBox, &type);
	winMsgBox.hauteur = -1;
	winMsgBox.largeur = width;
	winMsgBox.texte = texte;
	winMsgBox.titre = titre;
	winMsgBox.fctClose = fctClose;
	winMsgBox.fctHandle = fctHandle;
	winMsgBox.fctDestroy = NULL;
	winMsgBox.buttons = type;
	ShowWindow(&winMsgBox);
	return winMsgBox.selected;
}

void fctMsgBoxTemp_Close(WINDOW *w, int valid)		{

}

void fctMsgBoxTemp_Handle(WINDOW *w, int event)		{
	if (event == EVENT_HANDLE)		{
		if (w->userData[0]-- <= 0)
			CloseWindow(w, 0);
	}
}

void MessageBoxTemp(char *text, char *title, int type, int width, int time)		{
	MessageBoxAsync(text, title, type, width, fctMsgBoxTemp_Close, fctMsgBoxTemp_Handle);
	menuCurrentWindow->userData[0] = time;
}

void ShowMenuJoystickCalibration()			{
	char *directions[4] = {"up", "down", "left", "right"};
	int pass = 0, passokay = 0, curValue = 4000, skip = 0, fade = 0, i;
	s8 tempValues[4];
	menuSetStatusBarMessageDirect(NULL);
	while(!osl_quit)		{
		HandleBackground();
		if (!skip)		{
			menuSetStatusBarMessageDirect("\x12: Cancel");

			oslStartDrawing();
			DrawBackground();

	/*		oslDrawGradientRect(0,0,480,272,RGB(0,0,0), RGB(0,255,0), RGB(255,0,0), RGB(0,0,255));
			oslDrawGradientRect(0,0,480/2,272/2,RGBA(0,0,0,0), RGBA(0,0,0,0), RGBA(0,0,0,0), RGBA(0x80,0x80,0x80,0xff));
			oslDrawGradientRect(480/2,0,480,272/2,RGBA(0,0,0,0), RGBA(0,0,0,0), RGBA(0x80,0x80,0x80,0xff), RGBA(0,0,0,0));
			oslDrawGradientRect(0,272/2,480/2,272, RGBA(0,0,0,0), RGBA(0x80,0x80,0x80,0xff), RGBA(0,0,0,0), RGBA(0,0,0,0));
			oslDrawGradientRect(480/2,272/2,480,272,RGBA(0x80,0x80,0x80,0xff), RGBA(0,0,0,0), RGBA(0,0,0,0), RGBA(0,0,0,0));*/

			oslSetFont(ftGlow);
			MySetTextColorEx((menuColorSubmenuTitle & 0xffffff) | (fade << 24), 0x18);
			oslPrintf_xy(30, 30, "*** Calibration utility ***");
			MySetTextColorEx(RGBA(255, 255, 255, fade), 0x18);
			oslPrintf_xy(30, 50, "Press the joystick to the extreme %s and release it...", directions[pass]);
			oslSetFont(ftStandard);

			DrawBackgroundAfter();
			oslEndDrawing();
		}

		MyReadKeys();
		menuStandardVblank();
		
		//Annuler
		if (osl_keys->pressed.circle)
			break;
		//Up
		if (pass == 0)			{
			if (osl_keys->analogY == -128)
				passokay = 1;
			else if (passokay >= 1)			{
				if (abs(osl_keys->analogY - curValue) >= 5)				//Trop bougé
					curValue = osl_keys->analogY, passokay = 1;
				else		{
					if (osl_keys->analogY > curValue)
						curValue = osl_keys->analogY;
					passokay++;
				}
			}
		}
		//Down
		else if (pass == 1)			{
			if (osl_keys->analogY == 127)
				passokay = 1;
			else if (passokay >= 1)			{
				if (abs(osl_keys->analogY - curValue) >= 5)				//Trop bougé
					curValue = osl_keys->analogY, passokay = 1;
				else		{
					if (osl_keys->analogY < curValue)
						curValue = osl_keys->analogY;
					passokay++;
				}
			}
		}
		//Left
		else if (pass == 2)			{
			if (osl_keys->analogX == -128)
				passokay = 1;
			else if (passokay >= 1)			{
				if (abs(osl_keys->analogX - curValue) >= 5)				//Trop bougé
					curValue = osl_keys->analogX, passokay = 1;
				else		{
					if (osl_keys->analogX > curValue)
						curValue = osl_keys->analogX;
					passokay++;
				}
			}
		}
		//Right
		else if (pass == 3)			{
			if (osl_keys->analogX == 127)
				passokay = 1;
			else if (passokay >= 1)			{
				if (abs(osl_keys->analogX - curValue) >= 5)				//Trop bougé
					curValue = osl_keys->analogX, passokay = 1;
				else		{
					if (osl_keys->analogX < curValue)
						curValue = osl_keys->analogX;
					passokay++;
				}
			}
		}
		if (passokay >= 30)		{
			tempValues[pass++] = curValue;
			passokay = 0;
			curValue = 4000;
			//Finished
			if (pass >= 4)			{
				//Recopie dans le vrai tableau
				for (i=0;i<4;i++)
					menuConfig.ctrl.analog.calValues[i] = tempValues[i];
				menuConfig.ctrl.analog.useCalibration = 1;
				UpdateMenus(MENUUPD_CTRL);
				break;
			}
		}
		
		fade = min(fade + 16, 255);
		skip = oslSyncFrame();
	}
	osl_keys->pressed.value = 0;
	menuSetStatusBarMessageDirect(NULL);
}


void InitMenus()		{
	menuMainSoundStereoItems[1].disabled = 1;
	//Désactive les options inutiles (reset SMS et Screenshot)
	menuMainFileItems[1].disabled = !menuIsInGame;
	menuMainFileItems[2].disabled = !menuIsInGame;

//	menuMainMiscUIBackgroundItems[0].disabled = 1;
//	menuMainVideoCountryItems[1].disabled = 1;
}


SUBMENUITEM menuMsgBoxReloadConfigItems[]=		{
	{"Your default config", NULL, "", NULL, 0},
	{"System default config", NULL, "", NULL, 1},
	{"This game config", NULL, "", NULL, 2},
};

void LoadUserDefaultConfig()		{
	//Recopie la config. par défaut
	OuvreFichierConfig("default.ini", NULL);
	memcpy(menuConfigUserDefault, &menuConfig, sizeof(menuConfigUserDefault));
	memcpy(menuConfigUserMachineDefault, &menuConfig, sizeof(menuConfigUserMachineDefault));
}

void LoadDefaultMachineConfig()		{
	int success = 0;
	if (gblMachineType == EM_SMS)
		success = OuvreFichierConfig("default_sms.ini", NULL);
	else if (gblMachineType == EM_GBC)
		success = OuvreFichierConfig("default_gbc.ini", NULL);
	if (success)
		memcpy(menuConfigUserMachineDefault, &menuConfig, sizeof(menuConfigUserMachineDefault));
}

void SaveUserDefaultConfigSpecific()		{
	//Only save changes conpared to the default config for all machines.
	config_referenceConfig = menuConfigUserDefault;
	if (gblMachineType == EM_SMS)
		ScriptSave("default_sms.ini", SAVETYPE_GAME | SAVETYPE_CHANGESONLY);
	else if (gblMachineType == EM_GBC)
		ScriptSave("default_gbc.ini", SAVETYPE_GAME | SAVETYPE_CHANGESONLY);
	memcpy(menuConfigUserMachineDefault, &menuConfig, sizeof(menuConfigUserMachineDefault));
}

void SaveUserDefaultConfig()		{
	ScriptSave("default.ini", SAVETYPE_DEFAULT);
	memcpy(menuConfigUserDefault, &menuConfig, sizeof(menuConfigUserDefault));
}

int fctMsgBoxReloadConfig(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	if (event == EVENT_SET)		{
		menuMsgBoxReloadConfigItems[2].disabled = !menuIsInGame;
	}
	else if (event == EVENT_SELECT)			{
		if (!sub->disabled)		{
			//Sauver pour tous
			if (sub->prop1 == 1)
				//Recopie la config. par défaut
				memcpy(&menuConfig, menuConfigDefault, sizeof(menuConfig));
			else if (sub->prop1 == 0)		{
				LoadUserDefaultConfig();
				if (menuIsInGame)
					LoadDefaultMachineConfig();
			}
			else if (sub->prop1 == 2)			{
				char temp[MAX_PATH];
				
				//First load default
				LoadUserDefaultConfig();
				if (menuIsInGame)
					LoadDefaultMachineConfig();

				//Then load game-specific
				pspGetStateNameEx(menuFileSelectFileName, temp, STATE_CONFIG);
//				if (gblFlagLoadParams)
					OuvreFichierConfig(temp, NULL);
			}
			UpdateMenus(MENUUPD_ALL);
			CloseWindow(menuCurrentWindow, 1);
		}
		else
			return 0;
	}
	return 1;
}

SUBMENU menuMsgBoxReloadConfig=		{
	NULL, numberof(menuMsgBoxReloadConfigItems),
	1,			//Choice
	menuMsgBoxReloadConfigItems,
	fctMsgBoxReloadConfig
};

void fctMsgBoxReloadConfig_Close(WINDOW *w, int valid)		{

}

void fctMsgBoxReloadConfig_Handle(WINDOW *w, int event)		{
	if (event == EVENT_DRAW)		{
		int xx0, xx1, yy0, yy1, lm = LARGEUR_MENU;
		xx0 = (LARG - w->largeur) / 2;
		xx1 = xx0 + w->largeur;
		yy0 = (HAUT - w->hauteur) / 2;
		yy1 = yy0 + w->hauteur;
		LARGEUR_MENU = xx1 - xx0 - 26;
		DrawSubMenu(&menuMsgBoxReloadConfig, xx0 + 4, yy0 + 37, FALSE);
		LARGEUR_MENU = lm;
	}
	else if (event == EVENT_HANDLE)		{
		HandleSubMenu(&menuMsgBoxReloadConfig);
		//Ne jamais laisser gérer ça par la boîte sinon elle se fermera!
		osl_keys->pressed.cross = 0;
	}
}

SUBMENUITEM menuMsgBoxSaveConfigItems_noGame[]=		{
	{"All games", NULL, "", NULL, 1},
};

SUBMENUITEM menuMsgBoxSaveConfigItems_SMS[]=		{
	{"This game only", NULL, "", NULL, 0},
	{"All games", NULL, "", NULL, 2},
	{"All SMS games", NULL, "", NULL, 3},
};

SUBMENUITEM menuMsgBoxSaveConfigItems_GBC[]=		{
	{"This game only", NULL, "", NULL, 0},
	{"All games", NULL, "", NULL, 2},
	{"All GBC games", NULL, "", NULL, 3},
};

int fctMsgBoxSaveConfig(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	if (event == EVENT_SET)		{
		//Première possibilité dispo uniquement en jeu
		if (!menuIsInGame)
			menu->items = menuMsgBoxSaveConfigItems_noGame, menu->nbItems = numberof(menuMsgBoxSaveConfigItems_noGame);
		else if (gblMachineType == EM_SMS)
			menu->items = menuMsgBoxSaveConfigItems_SMS, menu->nbItems = numberof(menuMsgBoxSaveConfigItems_SMS);
		else if (gblMachineType == EM_GBC)
			menu->items = menuMsgBoxSaveConfigItems_GBC, menu->nbItems = numberof(menuMsgBoxSaveConfigItems_GBC);
	}
	else if (event == EVENT_SELECT)			{
		if (!sub->disabled)		{
			//Sauver pour tous
			if (sub->prop1 == 1)
				SaveUserDefaultConfig();
			else if (sub->prop1 == 2)		{
				if (MessageBoxEx("Do you want to remove machine specific configuration files? Your new settings will be the default for both SMS and GBC games.", "Question", MB_YESNO, 240))		{
					if (MessageBoxEx("Settings will be definately erased. Are you sure?", "Warning", MB_YESNO, 220))			{
						sceIoRemove("default_sms.ini");
						sceIoRemove("default_gbc.ini");
					}
				}
				SaveUserDefaultConfig();
			}
			else if (sub->prop1 == 3)
				SaveUserDefaultConfigSpecific();
			else if (sub->prop1 == 0)		{
				char temp[MAX_PATH];
				pspGetStateNameEx(menuConfig.file.filename, temp, STATE_CONFIG);
				//Save changes against the machine default
				config_referenceConfig = menuConfigUserMachineDefault;
				ScriptSave(temp, SAVETYPE_GAME | SAVETYPE_CHANGESONLY);
			}
			CloseWindow(menuCurrentWindow, 1);
		}
		else
			return 0;
	}
	return 1;
}

SUBMENU menuMsgBoxSaveConfig=		{
	NULL, numberof(menuMsgBoxSaveConfigItems_noGame),
	1,			//Choice
	menuMsgBoxSaveConfigItems_noGame,
	fctMsgBoxSaveConfig
};

void fctMsgBoxSaveConfig_Close(WINDOW *w, int valid)		{

}

void fctMsgBoxSaveConfig_Handle(WINDOW *w, int event)		{
	if (event == EVENT_DRAW)		{
		int xx0, xx1, yy0, yy1, lm = LARGEUR_MENU;
		xx0 = (LARG - w->largeur) / 2;
		xx1 = xx0 + w->largeur;
		yy0 = (HAUT - w->hauteur) / 2;
		yy1 = yy0 + w->hauteur;
		LARGEUR_MENU = xx1 - xx0 - 26;
		DrawSubMenu(&menuMsgBoxSaveConfig, xx0 + 4, yy0 + 37, FALSE);
		LARGEUR_MENU = lm;
	}
	else if (event == EVENT_HANDLE)		{
		HandleSubMenu(&menuMsgBoxSaveConfig);
		//Ne jamais laisser gérer ça par la boîte sinon elle se fermera!
		osl_keys->pressed.cross = 0;
	}
}

void menuUsbUpdate()		{
	if (!gbUsbAvailable)		{
		menuMainMiscUIItems[1].disabled = 1;
		strcpy(menuMainMiscUIItems[1].value, "(n/a)");
		menuMainMiscUIItems[1].description = "USB unavailable. You must be in kernel mode to enable USB!";

	}
	else	{
		menuMainMiscUIItems[1].disabled = 0;
		if (oslGetUsbState() & PSP_USB_ACTIVATED)
			strcpy(menuMainMiscUIItems[1].value, "On");
		else
			strcpy(menuMainMiscUIItems[1].value, "Off");
	}

}

void menuPlusShowMenu()
{
	float menuAngle, menuDesiredAngle;
	int nRayon, skip=0, lastOption, i, menuEndingFade = 0;
	static int bFirstTime = 1;
	char temp[200];
	
	//The menu runs at 60 fps, unlike PAL emulation
	oslSetFramerate(60);

	menuLoadFiles();

	if (menuIsInGame)
		menuIsInGame = 1;

	menuFrameNb = 0;
	menuMusicLocked = 0;
	fadeLevel = 0;
	fadeReason = 0;
	menuDesiredAngle = 360.f - (menuMainSelectedOption * 360.f / menuMain.nbItems);
	menuAngle = mod_f(menuDesiredAngle + 224.f, 360.f);
	nRayon = 240;
//	menuMainSelectedOption = 0;
	menuPlusTerminated = 0;
	menuCurrentWindow = NULL;
	InitHint(&gblHint);
	InitMenus();
	menuUsbUpdate();

	//D'abord aucun n'est affiché
	if (bFirstTime)		{
		for (i=0;i<menuMain.nbItems;i++)		{
			menuMain.items[i].subMenu->active = (i == menuMainSelectedOption) ? 1 : -1;
			menuMain.items[i].subMenu->moving = 0;
		}
	}

	//La première fois...
	if (!menuIsInGame)		{
		menuDefaultExecute[0] = 0;
		LoadUserDefaultConfig();
		LoadMyPlacesFile();
		OuvreFichierConfig("plugins.ini", "OnStart");
		ScriptAjouteProcedures("plugins.ini", &menuMainMiscCommands, NULL);
		//Exécute la routine par défaut
		if (menuDefaultExecute[0])
			OuvreFichierConfig("plugins.ini", menuDefaultExecute);
		GetSubMenuItemByInt(&menuMainMisc, 88)->disabled = (menuMainMiscCommands.nbItems <= 0);
		strcpy(GetSubMenuItemByInt(&menuMainMisc, 88)->value, (menuMainMiscCommands.nbItems <= 0) ? "" : ">>");
	}
	UpdateMenus(MENUUPD_ALL);

	oslSetKeyAutorepeatInit(24);
	oslSetKeyAutorepeatInterval(6);
	osl_keys->pressed.value = ~0;
	bFirstTime = 0;
	oslSetVSync(4);
	oslSetFrameskip(1);
	oslSetMaxFrameskip(MAX_FRAMESKIP);

	if (menuMusicState != 3)
		menuMusicPlay();
	menuSetStatusBarMessageDirect(NULL);

//	oslWaitKey();
	while (!osl_quit && !menuPlusTerminated)
	{
		//Configure le menu
		LARGEUR_MENU = 200;
		MENU_MAX_DISPLAYED_OPTIONS = 5;

		osl_keys->autoRepeatMask &= ~(OSL_KEYMASK_L | OSL_KEYMASK_R);
		MyReadKeys();

		//Debug mode
#ifdef DEBUG_MODE
		if (osl_keys->held.L && osl_keys->held.R && osl_keys->pressed.circle)			{
			if (osl_currentFrameRate >= 50)
				oslSetFramerate(8);
			else
				oslSetFramerate(60);
		}
		if (osl_keys->held.L && osl_keys->held.R && osl_keys->pressed.triangle)
			menuDebugRamShow = !menuDebugRamShow;
#endif

		menuStandardVblank();

		if (menuEndingFade)
			osl_keys->pressed.value = 0;
		if (menuFrameNb == 0)
			osl_keys->pressed.value = 0;
		HandleBackground();

		//Gestion de la sauvegarde des paramètres
		if (osl_keys->pressed.triangle)		{
			SUBMENU *sm = &menuMsgBoxSaveConfig;
			MessageBoxAsync("Save configuration for:", "Question", MB_OKCANCEL, 200, fctMsgBoxSaveConfig_Close, fctMsgBoxSaveConfig_Handle);
			if (menuIsInGame)
				menuCurrentWindow->hauteur += 49;
			else
				menuCurrentWindow->hauteur += 23;
			SelectSubMenu(sm, 1);
			sm->active = 1;
			sm->moving = 0;
			sm->close = 0;
		}

		if (menuGetMenuKey((u32*)&osl_keys->held.value, MENUKEY_MENU) && menuIsInGame)			{
			if (menuConfig.video.background == 1 || menuEndingFade)
				menuPlusTerminated = 1;
			else
				menuEndingFade = 1, menuFrameNb = (256 - 80) / 6;
			CloseChoice(&gblChoice);
			CloseWindow(menuCurrentWindow, 0);
		}
		//Fade de fin
		if (menuEndingFade)		{
			menuFrameNb -= 3;
			if (menuFrameNb <= 1)
				menuPlusTerminated = 1;
			nRayon += 10;
		}

		if (osl_keys->pressed.cross && nRayon > 0)		{
			//Stoppe les animations
			nRayon = 0;
			menuFrameNb = oslMax(300, menuFrameNb);
			menuAngle = menuDesiredAngle;
			osl_keys->pressed.cross = 0;
		}

		//Ne rien changer quand un menu de choix est actif
		if (!gblChoice.active)			{
			//Mouvement horizontal
			if (osl_keys->pressed.left || osl_keys->pressed.right)			{
				SUBMENU *s, *s2;
				int moving;
				//Stoppe l'animation d'ouverture
				nRayon = 0;
				menuFrameNb = oslMax(300, menuFrameNb);
				lastOption = menuMainSelectedOption;
				if (osl_keys->pressed.right)
					menuMainSelectedOption++, moving = 1;
				if (osl_keys->pressed.left)
					menuMainSelectedOption--, moving = -1;
				menuMainSelectedOption = menuMainSelectedOption % menuMain.nbItems;
				if (menuMainSelectedOption < 0)
					menuMainSelectedOption += menuMain.nbItems;
				menuDesiredAngle = 360.f - (menuMainSelectedOption * 360.f / menuMain.nbItems);
				s = menuMainItems[menuMainSelectedOption].subMenu;
				//Actif (dessiné) mais ne gère pas les touches (nouvelle option)
				s->active = 1;
				s->moving = moving;
				s->movingDist = 0;
				s->movingAlpha = 255;
				s->movingArrive = 140 * s->moving;
				s->close = 0;
				//Détruit les enfants
				s->child = NULL;
				//Trouve le plus petit enfant de l'ancienne option...
				s = menuMainItems[lastOption].subMenu;
				s2 = s;
				while (s->child)		{
					if (!s->child->close)
						s2 = s->child;
					s->close = 1;
					s->active = -1;
					s = s->child;
				}
				s2->active = 2;
				s2->moving = moving;
				s2->movingDist = 0;
				s2->movingAlpha = 255;
				s2->movingArrive = 140 * s->moving;
				s2->close = 1;
			}
		}

		//Première animation (entrée du menu en cercle)
		if (!menuEndingFade)
			nRayon -= 7;
		if (nRayon > 0)			{
			menuAngle = rangeAnglef(menuAngle + 4.f);
		}
		else		{
			float tmpMenuAngle;
			//Arrivée du cercle terminée
			nRayon = 0;
			tmpMenuAngle = adapteAnglef(menuDesiredAngle, menuAngle);
			//Animation de tournant terminée
//			if (oslAbs(tmpMenuAngle - menuDesiredAngle) < 0.5f)
//				moving = 0;
//				menuAngle = menuDesiredAngle;
			if (tmpMenuAngle < menuDesiredAngle)
				menuAngle += (menuDesiredAngle - tmpMenuAngle) / 5.f;
			else if (tmpMenuAngle > menuDesiredAngle)
				menuAngle -= (tmpMenuAngle - menuDesiredAngle) / 5.f;
		}
		//Gamme d'angle valable
		menuAngle = rangeAnglef(menuAngle);

		//Menu
//		if (moving)
//			HandleSubMenu(menuMainItems[lastOption].subMenu);
//		HandleSubMenu(menuMainItems[menuMainSelectedOption].subMenu);
		for (i=0;i<menuMain.nbItems;i++)
			HandleSubMenu(menuMainItems[i].subMenu);

		static int stateDisplayTimer = 0, stateLastOption;
		static SUBMENU *stateLastMenu;
		if (menuMainFileStateLoad.active == 1 || menuMainFileStateSave.active == 1 || menuMainFileStateDelete.active == 1)		{
			SUBMENU *s;
			if (menuMainFileStateLoad.active == 1)
				s = &menuMainFileStateLoad;
			if (menuMainFileStateSave.active == 1)
				s = &menuMainFileStateSave;
			if (menuMainFileStateDelete.active == 1)
				s = &menuMainFileStateDelete;
			if (s != stateLastMenu || stateLastOption != s->state.selection)
				stateDisplayTimer = 0;
			stateLastMenu = s;
			stateLastOption = s->state.selection;
			//<0 = désactivé, réactive si l'option a changé
			if (stateDisplayTimer >= 0)			{
				if (stateDisplayTimer == 15)		{
					if (!DoesSlotExists(s->items[s->state.selection].prop1))
						stateDisplayTimer = -1;
					else
						stateDisplayTimer++;
				}
				else
					stateDisplayTimer++;
				//Charge le state et l'image
				if (stateDisplayTimer == 40)		{
					//Pas de jeu chargé?
					if (DoesSlotExists(s->items[s->state.selection].prop1))			{
						int tmp = bitmap.depth;
						bitmap.depth = 16;
						bitmap.pitch  = 256 * bitmap.depth / 8;
						//Initialise le state temporaire
						if (!menuTempStateMemory)			{
							menuTempStateMemory = malloc(RAM_STATE_SIZE);
							pspSaveState(STATE_TEMP);
						}
						pspLoadState(s->items[s->state.selection].prop1);
						machine_frame(0);
						memcpy(ScreenTemp.scrBuffer, Screen.scrBuffer, SCR_BUFFER_SIZE);
						bitmap.depth = tmp;
						bitmap.pitch  = 256 * bitmap.depth / 8;
						pspLoadState(STATE_TEMP);
						machine_frame(0);
					}
				}
			}
		}
		else
			stateDisplayTimer = 0;

		//Ne pas afficher le hint pendant ce temps-là
		if (nRayon > 0)
			gblHint.active = 0;
		HandleHint(&gblHint);
		HandleChoice(&gblChoice);

		//Dessin
		if (!skip)		{
			menuSetStatusBarMessageDirect("\x13: Ok  \x12: Cancel  \x14: Default  \x11: Save config");

			oslStartDrawing();
			DrawBackground();

			menuDrawIcons(&menuMain, (int)menuAngle, nRayon);

			if (nRayon > 0)		{
				gblMenuAlpha = max(256 - (nRayon * 4), 0);
				oslSetAlpha(OSL_FX_ALPHA, gblMenuAlpha);
			}
			else
				gblMenuAlpha = 256;
			for (i=0;i<menuMain.nbItems;i++)
				DrawSubMenu(menuMainItems[i].subMenu, 100 - 4 - MENU_LEFT_MARGIN, 100, TRUE);

			if (stateDisplayTimer > 15)		{
				const int w = 200, h = 150;
				const int x = (LARG - CHOICE_MAX_POS) / 2 - w / 2, y = (HAUT - h) / 2;
				oslSetAlpha(OSL_FX_ALPHA, min((stateDisplayTimer - 15) * 16, 255));
				MyDrawRect(x - 1, y - 1, x + w + 1, y + h + 1, RGB(255, 255, 255));
				if (stateDisplayTimer > 40)			{
					int tmp = menuConfig.video.smoothing, tmp2 = bitmap.pal.update, tmp3 = bitmap.depth;
					menuConfig.video.smoothing = 1;
					bitmap.pal.update = 0;
					bitmap.depth = 16;
					bitmap.pitch  = 256 * bitmap.depth / 8;
					pspScaleImage((char *)ScreenTemp.scrBuffer,
									bitmap.viewport.x,
									bitmap.viewport.y,
									bitmap.viewport.x + bitmap.viewport.w,
									bitmap.viewport.y + bitmap.viewport.h,
									256,
									x, y, x + w, y + h, -1, 1);
					sceGuEnable(GU_DITHER);
					menuConfig.video.smoothing = tmp;
					bitmap.pal.update = tmp2;
					bitmap.depth = tmp3;
					bitmap.pitch  = 256 * bitmap.depth / 8;
				}
				else		{
					MyDrawFillRect(x, y, x + w, y + h, RGB(0, 0, 0));
					oslSetTextColor(RGB(255, 255, 255));
					oslDrawString(x + w / 2 - GetStringWidth("Preview...") / 2, y + h / 2 - 6, "Preview...");
				}
			}
			oslSetAlpha(OSL_FX_DEFAULT, 0);

			DrawHint(&gblHint);
			DrawChoice(&gblChoice);
			DrawBackgroundAfter();

			oslEndDrawing();
		}

		if (menuFrameNb == 0)
			osl_vblCallCount = osl_vblCount;
		skip = oslSyncFrame();
	}
	//Clean-up des menus
	CloseChoice(&gblChoice);
	gblChoice.position = 0;
/*	for (i=0;i<menuMain.nbItems;i++)			{
		SUBMENU *s, *ss;
		s = menuMainItems[i].subMenu;
		do		{
			s->parent = NULL;
			s->close = 1;
			s->active = -1;
			ss = s;
			s = s->child;
			ss->child = NULL;
		} while (s);
	}*/
	oslSetFont(ftStandard);
	//Plus besoin de ce save state
	if (menuTempStateMemory)		{
		free(menuTempStateMemory);
		menuTempStateMemory = NULL;
	}
	menuMusicPause();
	menuSetStatusBarMessageDirect(NULL);
}

void SetTurbo(int value)		{
	menuConfig.video.turbo = value;
	if (menuConfig.video.turbo)			{
		//Disable sound during turbo
		if ((!menuConfig.sound.turboSound /*!= 2*/ || !soundInited) && menuConfig.sound.enabled)		{
			//Initialize sound if it has not be done before
			if (!soundInited && menuConfig.sound.turboSound /*!= 0*/)			{
				snd.enabled = 1;
//				snd.sample_rate = menuConfig.sound.sampleRate;
//				sound_init();
				SoundInit();
			}

			//Force off
			if (!menuConfig.sound.turboSound /*== 0*/)			{
				SoundTerm();
				snd.enabled = 0;
//				snd.sample_rate = 0;
//				sound_init();
			}
			else
				SoundPause();
		}
	}
	else		{
		if ((menuConfig.sound.turboSound /*!= 2*/ || !soundInited) && menuConfig.sound.enabled)		{
			//Force off
			if (!menuConfig.sound.turboSound /*== 0*/ || !soundInited)			{
				if (gblMachineType == EM_SMS)
					snd.sample_rate = menuConfig.sound.sampleRate;
				else if (gblMachineType == EM_GBC)
					snd.sample_rate = 44100;
				sound_change();
				SoundInit();
			}
			else
				SoundResume();
		}
	}
}

u32 ControlsMenuUpdate(u32 buttons)		{
	char *statetext = NULL;
	menuGetMenuKeys(&buttons);

	//Options du menu
	if (menuGetMenuKey(&buttons, MENUKEY_MENU))
		ControlsShowMenu();
	if (menuGetMenuKey(&buttons, MENUKEY_TURBO))		{
		char str[100];
		//Toggle turbo
		SetTurbo(!menuConfig.video.turbo);
		strcpy(str, "Turbo mode ");
		if (menuConfig.video.turbo)
			strcat(str, "on");
		else
			strcat(str, "off");
		menuDisplayStatusMessage(str, -1);
	}
	if (menuGetMenuKey(&buttons, MENUKEY_PAUSE))		{
		menuConfig.video.pause = !menuConfig.video.pause;
		if (menuConfig.video.pause)		{
			menuDisplayStatusMessage("Game paused", -1);
			SoundPause();
		}
		else	{
			menuDisplayStatusMessage("Game unpaused", -1);
			SoundResume();
		}
	}
	if (menuGetMenuKey(&buttons, MENUKEY_SLOAD))		{
		SoundPause();
			if (pspLoadState(menuConfig.file.state))
				statetext = "State loaded";
		SoundResume();
	}
	if (menuGetMenuKey(&buttons, MENUKEY_SSAVE))		{
		SoundPause();
			if (pspSaveState(menuConfig.file.state))
				statetext = "State saved";
		SoundResume();
	}
	if (menuGetMenuKey(&buttons, MENUKEY_SPLUS))		{
		menuConfig.file.state = mod(menuConfig.file.state + 1, 11);
		statetext = "Current state";
	}
	if (menuGetMenuKey(&buttons, MENUKEY_SMINUS))		{
		menuConfig.file.state = mod(menuConfig.file.state - 1, 11);
		statetext = "Current state";
	}
	if (statetext)		{
		char str[100];
		if (menuConfig.file.state == STATE_RAM)
			sprintf(str, "%s: RAM", statetext);
		else
			sprintf(str, "%s: Slot %i", statetext, menuConfig.file.state);
		menuDisplayStatusMessage(str, -1);
	}
	if (menuGetMenuKey(&buttons, MENUKEY_RESET))		{
//		if (MessageBox("Do you want to reset the console?", "Question", MB_YESNO))			{
		machine_reset();
		menuDisplayStatusMessage("Emulation reseted", -1);
//		}
	}
	if (menuGetMenuKey(&buttons, MENUKEY_SCREENSHOT))		{
		menuTakeScreenshot();
		menuDisplayStatusMessage("Screenshot saved", -1);
		//Ne pas garder le state en mémoire quand on est ingame
		if (menuTempStateMemory)		{
			free(menuTempStateMemory);
			menuTempStateMemory = NULL;
		}
	}
	return buttons;
}

int menuSoundBoxWidth[3], menuSoundBoxOpen = 0;
float menuSoundBoxPosition = 0;

void menuStandardVblank()			{
	if (musicEnabled)			{
		int pressed, next;
		soundStandardVblank();

		if (menuGetMenuKey((u32*)&osl_keys->held.value, MENUKEY_MUSICPLAYER))		{
			osl_keys->pressed.value = 0;
			menuSoundBoxOpen = !menuSoundBoxOpen;
			menuSoundBoxActive = 0;
			menuSetStatusBarMessageDirect(NULL);
		}
		if (menuSoundBoxOpen && !menuCurrentWindow)		{
			char str[16];
			char str2[100];
			if (!menuMusicLocked)		{
				if (osl_keys->pressed.R)
					menuMusicNextTrack();
				if (osl_keys->pressed.circle)			{
					//Lock
					menuMusicLocked = 1;
					//Spare some ressources
					oslSetMaxFrameskip(0);
					menuSetStatusBarMessageDirect(NULL);
				}
				if (osl_keys->pressed.cross)			{
					//Paused
					if (menuMusicState == 2 || menuMusicState == 3)
						menuMusicPlay();
					else		{
						menuMusicPause();
						//Don't continue the music when returning to the menu
						menuMusicState = 3;
					}
				}
				if (osl_keys->pressed.L)
					menuMusicPrevTrack();
			}
			else		{
				int combination = OSL_KEYMASK_L | OSL_KEYMASK_R | OSL_KEYMASK_START;
				int i;

				for (i=0;i<3;i++)			{
					oslReadKeys();
					if ((osl_keys->lastHeld.value & combination) == combination)		{
	/*					while (osl_keys->lastHeld.value & combination)		{
							MyReadKeys();
							oslWaitVSync();
						}*/
						menuMusicLocked = 0;
						menuSetStatusBarMessageDirect(NULL);
						oslSetMaxFrameskip(MAX_FRAMESKIP);
						break;
					}
					//1 second here, waiting => spare resource
					oslWaitVSync();
				}
			}

			if (menuMusicLocked)
				strcpy(str2, "L+R+Start: Unlock");
			else
				sprintf(str2, "L: Prev track  R: Next  \x13: Pause  \x12: Lock  %s: Close", menuGetKeyName(str, sizeof(str), "+", menuConfig.ctrl.cuts.musicplayer));
			menuSetStatusBarMessageIndirect(str2);

			gblShortcutKey = -1;
			osl_keys->held.value = 0;
			osl_keys->pressed.value = 0;
		}
	}
	else		{
		if (menuSoundBoxOpen)		{
			menuSoundBoxOpen = 0;
			menuSetStatusBarMessageDirect(NULL);
		}
	}
}

void menuSoundShowDisplay()			{
	float pos;
	if (menuSoundBoxActive > 0 || menuSoundBoxOpen)		{
		//La première fois, calcule la taille max
		if (menuSoundBoxActive == MENU_SOUND_BOX_ACTIVE_MAX)		{
			menuSoundBoxWidth[0] = GetStringWidth(menuSoundTitle);
			menuSoundBoxWidth[1] = GetStringWidth(menuSoundArtist);
//			menuSoundBoxWidth[2] = GetStringWidth(menuSoundAlbum);
		}
		pos = 40.0f;
		menuSoundBoxActive--;
	}
	else
		pos = 0.0f;
	menuSoundBoxPosition += (pos - menuSoundBoxPosition) / 3.0f;
	if (menuSoundBoxPosition > 0)			{
		int position = menuSoundBoxPosition, width;
		width = min(max(menuSoundBoxWidth[0] + 21, menuSoundBoxWidth[1] + 21), 220);
		oslSetTextColor(RGB(255, 255, 255));
		mySetScreenClipping(10, 259 - position, 10 + width, 259);
		MyDrawRect(10, 259 - position, 10 + width, 259 - position + 34, RGBA(255, 255, 255, 128));
		MyDrawFillRect(11, 259 - position + 1, 10 + width - 1, 259 - position + 34 - 1, RGBA(0, 0, 0, 128));
		oslDrawString((10 + (10 + width)) / 2 - menuSoundBoxWidth[0] / 2, 259 - position + 3, menuSoundTitle);
		oslDrawString((10 + (10 + width)) / 2 - menuSoundBoxWidth[1] / 2, 259 - position + 16, menuSoundArtist);
//		oslDrawString((10 + (10 + width)) / 2 - menuSoundBoxWidth[2] / 2, 259 - position + 30, menuSoundAlbum);
		oslResetScreenClipping();
	}
}
