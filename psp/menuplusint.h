#define MAX_FRAMESKIP 4

void menuSoundShowDisplay();
void SelectSubMenuItemPosition(SUBMENU *s, int pos);
int SubMenuItemPositionSelected(SUBMENU *menu, SUBMENUITEM *item);
int SubMenuItemPositionCanceled(SUBMENU *menu, SUBMENUITEM *item);
void UpdateMenus(int elements);
char SelectSubMenuItemPositionByValueInt(SUBMENU *menu, int param);
void SelectSubMenu(SUBMENU *s, char isNew);
void CreateDigitsFromInt(SUBMENUMINMAX *mm, int value, int fix);
void CreateDigitsFromStr(SUBMENUMINMAX *mm, char *str, int fix);
int GetIntFromDigits(SUBMENUMINMAX *mm);
void CloseChoice(CHOICEMENU *h);
void fadeInit(int value, int reason);
void CloseWindow(WINDOW *w, int valid);
SUBMENUITEM *GetSubMenuItemByInt(SUBMENU *menu, int value);
void ShowMenuJoystickCalibration();
void DrawBackground();
void DrawBackgroundAfter();
void menuSetStatusBarMessageDirect(char *message);
void menuSetStatusBarMessageIndirect(char *message);
void *menuRamStateMemory, *menuTempStateMemory;
char menuRamStateFileName[MAX_PATH];
void DrawWindow(WINDOW *w);
void HandleWindow(WINDOW *w);
void menuStandardVblank();
void menuGetMenuKeys(u32 *pad);
void LoadDefaultMachineConfig();
void ShowMenuFileSelect(char *savepath, int pathSlot);


extern int LARGEUR_MENU;
extern int MENU_MAX_DISPLAYED_OPTIONS;
extern screen_t Screen __attribute__((aligned(64)));

extern OSL_IMAGE *imgIcons, *imgNumbers, *imgBord, *imgBack;
extern OSL_FONT *ftStandard, *ftGlow;
extern HINT gblHint;
extern CHOICEMENU gblChoice;
//extern MENUPARAMS menuConfig, menuConfigDefault, menuConfigUserDefault;
extern WINDOW *menuCurrentWindow;
extern WINDOW winMsgBox;
extern int fadeLevel, fadeDirection, fadeReason, menuAnimateCursor;
extern SceIoDirent dirEntry;
extern char menuFileSelectPath[MAX_PATH];
extern char menuFileSelectFileName[MAX_PATH];
extern int menuPlusTerminated, menuMainSelectedOption;
extern MENUPRESSEDKEYS menuPressedKeys;
extern int menuPressedKeysAutorepeatInit, menuPressedKeysAutorepeatDelay, menuPressedKeysAutorepeatMask;
extern char menuStatusMessage[200];
extern int menuStatusActive;
extern int menuStatusPosition, menuStatusDrawn;
extern int menuStickMessageDisplayed;
extern int gblMenuAlpha, menuFrameNb, frameCount;
extern int menuIsInGame, menuUpdateRender;
extern char *menuStatusBarMessage;
extern char menuStatusBarMessageInd[256];
extern int gblFlagLoadParams;
extern u32 gblShortcutKey;
extern char menuTempMessageText[256];
extern int menuMusicLocked;
extern int gblConfigAutoShowCrc, gblModeColorIt;

typedef struct		{
	char *szExt;
	int nExtId;
} STEXTENSIONLIST;
extern STEXTENSIONLIST *stExtentions;
extern STEXTENSIONLIST stRomExtentions[], stMusicExtentions[];

#define MSGBOX_MAX_MULTIPLY 256
