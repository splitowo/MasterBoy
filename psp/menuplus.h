#ifdef __cplusplus
extern "C" {
#endif

#define LARG 480
#define HAUT 272
//Transparence max du curseur
#define STATE_ACTIVE_MAX 224
//En mode numérique
#define NB_MAX_DIGITS 5
#define MENU_LEFT_MARGIN 8
#define HAUTEUR_TEXTE 14
#define HAUTEUR_DIGIT 16
#define LARGEUR_DIGIT 16
//Valeur de l'option par défaut des menus
#define MASK_DEFAULT_MENU_VALUE 0x8000
#define CHOICE_MAX_POS 170
#define RAM_STATE_SIZE (128 << 10)

#define NBRE_TOUCHES 10
#define NBRE_CUTS 10

char gblRomPath[MAX_PATH];
char gblMusicPath[MAX_PATH];

typedef struct		{
	struct			{
		char filename[MAX_PATH];
		int state;
		int sramAutosave;
		int wifiEnabled;
		int wifiServer;
	} file;

	struct			{
		char filename[MAX_PATH];
		int frequency;
		int repeatMode;
	} music;

	struct			{
		int country;				// 0 = NTSC, 1 = PAL
		int render;					// 0 = 1:1, 1 = Fit, 2 = Full
		int smoothing;				// 0 = off, 1 = on
		int frameskip;				// 0 = auto, 1-10: 0-9
		int frameskipMax;
		int vsync;					// 0 = auto, 1 = off, 2 = on
		int turboSkip;
		int background;				// 0 = in-game, 1 = bleu
		int brightnessMode;			// 0 = normal, 1 = bright & fast, 2 = gamma
		int gamma;					// 100 = normal (1.00)
		int vibrance;				// 0 - 256 (128 = normal)
		int renderDepth;			// 8 = 8 bits, 16 = 16 bits
		int cpuTime;				// 0 = off, 1 = on
		int leftBar;				// 0 = off, 1 = on, 2 = auto
		int spriteLimit;			// 0 = off, 1 = on
		int turbo, pause;
		int syncMode;				// 0 = tight, 1 = loose
	} video;

	struct			{
		int enabled;
		int sampleRate;				// 44100, 22050, 11025
		int stereo;					// 0 = stereo, 1 = mono
		int volume;					// 1 = plus fort, 0 = normal
		int turboSound;				// 0 = no sound in turbo mode, 1 = sound enabled
		int perfectSynchro;
	} sound;

	struct			{
		//Config analogique
		struct		{
			//Marge depuis le point milieu
			int toPad;
			int treshold;
			int useCalibration;
			//Valeurs calibrées
			int calValues[4];
		} analog;
		union		{
			u32 akeys[NBRE_TOUCHES];
			struct		{
				u32 up, down, left, right, button1, button2, start, auto1, auto2, select;
			} keys;
		};
		union		{
			u32 acuts[NBRE_CUTS];
			struct		{
				u32 menu, turbo, pause, sload, ssave, splus, sminus, reset, musicplayer, screenshot;
			} cuts;
		};
		int autofireRate;
	} ctrl;

	struct			{
		int pspClock;					// en MHz
		int z80Clock;					// 0 = normal
		int SMSLines;					// 0 = normal
	} misc;

	struct		{
		int gbType;						// 0 = AUTO, 1 = DMG, 2 = SGB, 3 = CGB, 4 = AGB
		int colorization;				// 0 = off, 1 = on
		char palette[32];				// Nom de la palette
	} gameboy;

} MENUPARAMS;

typedef struct			{
	union		{
		u32 acuts[NBRE_CUTS];
	};
} MENUPRESSEDKEYS;

//MENUKEY_LAST_ONE must always stay the last one!
enum {MENUKEY_MENU = 0, MENUKEY_TURBO, MENUKEY_PAUSE, MENUKEY_SLOAD, MENUKEY_SSAVE, MENUKEY_SPLUS, MENUKEY_SMINUS, MENUKEY_RESET, MENUKEY_MUSICPLAYER, MENUKEY_SCREENSHOT, MENUKEY_LAST_ONE};
#define MENUKEY_AUTOREPEAT_MASK ((1<<5) | (1<<6))
#define STATE_SRAM -1
#define STATE_RAM 10
#define STATE_EXPORT 11
#define STATE_TEMP 0xff
#define STATE_CONFIG 0xfe

enum actions {MA_LOADROM};
#define min(x, y)		(((x) < (y)) ? (x) : (y))
#define max(x, y)		(((x) > (y)) ? (x) : (y))
#define numberof(a)		(sizeof(a)/sizeof(*(a)))

extern int MenuPlusAction(int action, void* param);
extern void menuPlusShowMenu();
extern void UpdateMenus(int elements);
//Retourne si une touche est pressée ou non
extern int menuGetMenuKey(u32 *pad, int key);
extern void menuDisplayStatusMessage(char *message, int time);
extern u32 ControlsMenuUpdate(u32 buttons);
extern void menuKeysAnalogApply(u32 *keys, int aX, int aY);
extern OSL_CONTROLLER *MyReadKeys();
extern int MessageBoxEx(const char *texte, const char *titre, int type, int width);
extern void safe_strcpy(char *dst, const char *src, u32 maxlen);
extern void safe_strcat(char *dst, const char *src, u32 maxlen);
extern int BatteryWarning(const char *message);
#define MessageBox(texte, titre, type)		MessageBoxEx(texte, titre, type, 200)
extern void MessageBoxTemp(char *text, char *title, int type, int width, int time);
extern int menuPlusTerminated;
extern int menuStatusPosition, menuStatusDrawn;
void SaveMyPlacesFile();
enum {MB_NONE=0, MB_OK=1, MB_CANCEL=2, MB_OKCANCEL=3, MB_YESNO=4};
enum {FALSE=0, TRUE=1};

extern MENUPARAMS menuConfig;
extern MENUPRESSEDKEYS menuPressedKeys;
extern int menuPressedKeysAutorepeatInit, menuPressedKeysAutorepeatDelay, menuPressedKeysAutorepeatMask, menuPressedKeysAutorepeat;
extern int menuIsInGame, menuUpdateRender, menuAnimateCursor;
extern u32 menuColorCursor[4], menuColorSubmenuTitle;
extern int soundInited;

/*
	TYPES
*/
//TOUT INITIALISER, surtout les states!!!!!!!!!
typedef struct		{
	s16 selection;
	float selPos;
	u8 selectionDirect;
	s16 active;
	s16 scroll;
	float scrollPos;
	u8 alphaFleches[2];
} SUBMENUSTATE;

typedef struct		{
	int min, max, width;
	char value[20];
	int flecheX;
	float flechePosX;
	s8 flecheAnim;
	float digitsPos[NB_MAX_DIGITS];
	s8 digits[NB_MAX_DIGITS];
} SUBMENUMINMAX;

typedef struct SUBMENUITEM 		{
	char *name;
	char *description;
	char value[20];
	struct SUBMENU *link;
	int prop1, prop2;					//Propriétés (perso)
	char disabled;						// 0 = actif, 1 = inactif, +2 = couleur spéciale (prop2)
} SUBMENUITEM;

typedef struct SUBMENU		{
	const char *titre;
	u16 nbItems;
	u8 type;						// 1 = sous-menu normal, 2 = choix
	SUBMENUITEM *items;
	//Menu, item, event. EVENT_SELECT => retourner 1 pour fermer, 0 sinon
	int (*fctGestion)(struct SUBMENU*, SUBMENUITEM*, u32);
	void *data;

	//Variable
	SUBMENUSTATE state;
	s8 active;
	float xOffset, xOffset2;
	u8 close;
	s16 moving, movingDist, movingArrive, movingAlpha;

	struct SUBMENU *child, *parent;
} SUBMENU;

typedef struct		{
	int sizeX, sizeY;
} MENURETURN;

typedef struct		{
	int nbItems;
	struct MENUITEM *items;
} MENU;

typedef struct MENUITEM		{
	int iconOffsetY;
	SUBMENU *subMenu;
} MENUITEM;

typedef struct		{
	int active;
	float position;
	const char *text;
} HINT;

typedef struct		{
	int active;
	float position;
	SUBMENU *submenu, *parent;
	int type;					//1 = menu simple
} CHOICEMENU;

typedef struct WINDOW		{
	s16 largeur, hauteur;
	const char *titre;
	const char *texte;
	const char *button1;
	const char *button2;
	//Fonction qui sera appelée à la fermeture (mettre NULL pour bloquer et retourner un résultat)
	void (*fctClose)(struct WINDOW*, int);
	//Message: EVENT_INIT, EVENT_DRAW, EVENT_HANDLE
	void (*fctHandle)(struct WINDOW*, int);
	int buttons;
	void (*fctDestroy)(struct WINDOW*);

	//Variable
	s8 state;					// -1 invisible, 0 closed, 1 visible
	s16 multiply;
	s8 selected;
	u32 userData[2];
} WINDOW;

#define SCR_BUFFER_SIZE (256*256*2)
enum {ACTION_DRAW, ACTION_HANDLE};
enum {EVENT_INIT, EVENT_SET, EVENT_SELECT, EVENT_CANCEL, EVENT_DRAW, EVENT_HANDLE, EVENT_DEFAULT};
enum {MENUUPD_FILE=1, MENUUPD_VIDEO=2, MENUUPD_SOUND=4, MENUUPD_CTRL=8, MENUUPD_MISC=16,
		MENUUPD_ALL=31};
enum { 
    PSP_FILE_TYPE_DIR=0x10, 
    PSP_FILE_TYPE_FILE=0x20 
}; 
enum {EXT_UNKNOWN, EXT_DIR, EXT_ZIP, EXT_SMS, EXT_GG, EXT_GB};

extern int ShowChoiceMsgBox(char *msg, char *title, int buttons, int width, char *elements[], int nbElements, int blocking);
extern int ShowChoiceMsgBoxEx(char *msg, char *title, int buttons, int width, SUBMENU *sm, int blocking);
void menuUsbUpdate();

void menuSetStatusBarMessageDirect(char *message);
int GetStringWidth(const char *str);
void MyDrawRect(int x0, int y0, int x1, int y1, OSL_COLOR color);
void MyDrawFillRect(int x0, int y0, int x1, int y1, OSL_COLOR color);
int machine_manage_sram(int mode, int force);
void OuvreFichierPalette(int crc, char *function);
char *extractFilePath(char *file, char *dstPath, int level);
int tileMapGetVramChecksum();
int fileExists(char *fileName);
void makeDir(char *dir);
void menuShowPaletteList();
void pspGetStateName(char *name, int slot);
void pspGetStateNameEx(char *romFile, char *name, int slot);
extern int gblConfigAutoShowCrc, gblModeColorIt;
extern int gb_lastVramCrc;
extern int gbUsbAvailable;
extern OSL_COLOR currentPalCache[12];
extern const OSL_COLOR defaultPalCache[12];
void StandardSoundProcess();
int MessageBoxAsync(const char *texte, const char *titre, int type, int width, void (*fctClose)(struct WINDOW*, int), void (*fctHandle)(struct WINDOW*, int));
extern void SoundPause();
extern void SoundResume();
extern int menuDisplaySpecialMessage;
extern unsigned char snd_stat_upd;

MENUPARAMS *menuConfigDefault, *menuConfigUserDefault, *menuConfigUserMachineDefault;

//For game boy ColorIt.
#define NB_PALETTES 48

#ifdef __cplusplus
}
#endif
