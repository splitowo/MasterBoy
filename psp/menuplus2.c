#include "pspcommon.h"
#include "specialtext.h"
#include "sound.h"
#include "menuplusint.h"

int menuFileSelectCurrentMenu = 0, menuFileSelectSwap;
//-1: menu currently running, 0: user cancelled, >= 1: choice selected
static int gblMsgBoxChoiceValue;
static SUBMENU choiceMsgBoxMenu;

#define PATHSLOT_BASE 2

int nbPlaces = 0;
//Fixé: le 0 est le last path.
char (*cPlaces)[MAX_PATH] = NULL;
//1 = dir, 2 = file
int *nPlacesTypes = NULL;
int myPlacesModified = 0;

int myPlacesRealloc(int number)		{
	char (*temp)[MAX_PATH];
	int *temp2;
	temp = realloc(cPlaces, MAX_PATH * number);
	temp2 = realloc(nPlacesTypes, number * sizeof(*nPlacesTypes));
	if (!temp || !temp2)		{
		if (cPlaces)
			free(cPlaces);
		if (nPlacesTypes)
			free(nPlacesTypes);
		cPlaces = NULL;
		nPlacesTypes = NULL;
		nbPlaces = 0;
		return 0;
	}
	cPlaces = temp;
	nPlacesTypes = temp2;
	nbPlaces = number;
	return 1;
}

void InitMyPlacesBase()		 {
	strcpy(cPlaces[0], gblRomPath);
	strcpy(cPlaces[1], gblMusicPath);
	nPlacesTypes[0] = nPlacesTypes[1] = 1;
}

void LoadMyPlacesFile()		{
	int i;
	myPlacesModified = 0;

	VIRTUAL_FILE *f;
	f = VirtualFileOpen("romdir.ini", 0, VF_FILE, VF_O_READ);

	i = PATHSLOT_BASE;
	if (!myPlacesRealloc(i))
		return;

	InitMyPlacesBase();

	if (f)			{
		while (!VirtualFileEof(f))			{
			char str[MAX_PATH + 10];
			VirtualFileGets(str, sizeof(str), f);

			if (!strncmp(str, "rom=", 4))		{
				if (!myPlacesRealloc(i + 1))
					break;
				nPlacesTypes[i] = 2;
				strcpy(cPlaces[i], str + 4);
				i++;
			}
			else if (!strncmp(str, "dir=", 4))			{
				if (!myPlacesRealloc(i + 1))
					break;
				nPlacesTypes[i] = 1;
				strcpy(cPlaces[i], str + 4);
				i++;
			}
			else if (!strncmp(str, "lastrom=", 8))			{
				nPlacesTypes[0] = 1;
				strcpy(cPlaces[0], str + 8);
			}
			else if (!strncmp(str, "lastzik=", 8))			{
				nPlacesTypes[1] = 1;
				strcpy(cPlaces[1], str + 8);
			}
		}
		VirtualFileClose(f);
	}
}

void SaveMyPlacesFile()		{
	VIRTUAL_FILE *f;
	if (!myPlacesModified || nbPlaces == 0)
		return;
	f = VirtualFileOpen("romdir.ini", 0, VF_FILE, VF_O_WRITE);
	if (f)			{
		int i;
		for (i=0;i<nbPlaces;i++)		{
			if (i == 0)
				VirtualFilePuts("lastrom=", f);
			else if (i == 1)
				VirtualFilePuts("lastzik=", f);
			else if (nPlacesTypes[i] == 1)
				VirtualFilePuts("dir=", f);
			else if (nPlacesTypes[i] == 2)
				VirtualFilePuts("rom=", f);
			VirtualFilePuts(cPlaces[i], f);
			VirtualFilePuts("\r\n", f);
		}
		VirtualFileClose(f);
	}
}

void AddToMyPlaces(const char *dirName, int type)		{
	int newFile = 0, i;

	//First verify if it already exists
	for (i=PATHSLOT_BASE;i<nbPlaces;i++)		{
		if (!strcmp(dirName, cPlaces[i]))
			return;
	}

	if (nbPlaces <= PATHSLOT_BASE)		{
		newFile = 1;
		if (!MessageBoxEx("A my places configuration file will be created. MasterBoy will then display the contents of my places when you choose to Load ROM instead of the usual ROM directory. Do you want to continue?", "Question", MB_YESNO, 300))
			return;
		nbPlaces = 2;
	}
	nbPlaces++;
	if (myPlacesRealloc(nbPlaces))			{
		//The entry 0 must be the ROM directory name (and 1 the music)
		if (newFile)		{
			InitMyPlacesBase();
		}
		strcpy(cPlaces[nbPlaces-1], dirName);
		nPlacesTypes[nbPlaces-1] = type;
		myPlacesModified = 1;
	}
}

void SetMyPlacesEntry(int entry, const char *dirName, int type)		{
	if (entry < 0 || entry >= nbPlaces)
		return;
	strcpy(cPlaces[entry], dirName);
	nPlacesTypes[entry] = type;
	myPlacesModified = 1;
}

void RemoveMyPlacesEntry(int entry)		{
	if (entry < 0 || entry >= nbPlaces)
		return;

	myPlacesModified = 1;

	//Do not delete the "recent:" element
	if (entry < PATHSLOT_BASE)			{
		cPlaces[entry][0] = 0;
		nPlacesTypes[entry] = 0;
		return;
	}

	if (entry < nbPlaces - 1)			{
		memmove(&cPlaces[entry], &cPlaces[entry+1], sizeof(cPlaces[entry]) * (nbPlaces - entry - 1));
		memmove(&nPlacesTypes[entry], &nPlacesTypes[entry+1], sizeof(nPlacesTypes[entry]) * (nbPlaces - entry - 1));
	}
	nbPlaces--;
}

int fctMenuFileSelect(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	if (event == EVENT_SELECT)			{
		menuFileSelectSwap = 1;
		return 0;
	}
	if (event == EVENT_CANCEL)		{
		menuFileSelectSwap = -1;
		return 0;
	}
	return 1;
}

//Thanks to DGEN author ;)
void SJISCopy(SUBMENUITEM *a, unsigned char *file)
{
	unsigned char ca;
	int i;

	for(i=0;a->name[i]!=0;i++)
	{
		ca = a->name[i];
		if( ((0x81 <= ca)&&(ca <= 0x9f)) || ((0xe0 <= ca)&&(ca <= 0xef)) )
		{
			file[i++] = ca ;
			file[i  ] = a->name[i];
		}
		else
		{
			if(ca>='a' && ca<='z') ca -= 0x20 ;
			file[i] = ca;
		}
	}
}

int menuFileItemCompare(SUBMENUITEM *a, SUBMENUITEM *b)
{
    unsigned char file1[0x108];
    unsigned char file2[0x108];
	unsigned char ca, cb;
	int i, n, ret;

	if( (a->prop1 & 0x30) == (b->prop1 & 0x30) )
	{
		SJISCopy(a, file1);
		SJISCopy(b, file2);
		for(i=0; file1[i]; i++)
		{
			ca = file1[i] ; 
			cb = file2[i] ;
			ret = ca - cb ;
			if( ret != 0 ) return ret;
		}
		return 0;
	}
	
	if (a->prop1 & PSP_FILE_TYPE_DIR) return -1 ;
	else					          return  1 ;
}

void menuFileSelectSortFiles(SUBMENU *s, int left)
{
	SUBMENUITEM tmp;
	int n, m;
	int right = s->nbItems;

	for (n=left;n<right-1;n++)
	{
		for(m=n+1;m<right;m++)
		{
			if (menuFileItemCompare(&s->items[n], &s->items[m]) > 0)
			{
				tmp = s->items[n];
				s->items[n] = s->items[m];
				s->items[m] = tmp;
			}
		}
	}
}

int getExtId(const char *szFilePath) 
{
	char *pszExt;
	int i;
	if((pszExt = strrchr(szFilePath, '.'))) 
	{
		pszExt++;
		for (i = 0; stExtentions[i].nExtId != EXT_UNKNOWN; i++) 
		{
			if (!strcasecmp(stExtentions[i].szExt,pszExt)) 
			{
				return stExtentions[i].nExtId;
			}
		}
	}
	return EXT_UNKNOWN;
}

static void freeSubMenuItems(SUBMENU *s)
{
	int i;
	for(i = 0; i < s->nbItems; i++)
	{
		free(s->items[i].name);
	}
	if(s->items) free(s->items);
	s->items = NULL;
}

int menuFileSelectFillFiles(SUBMENU *s)		{
	int fd, ext_id, nbFiles = 0;

	freeSubMenuItems(s);

	fd = sceIoDopen(menuFileSelectPath);
	if (fd < 0)		{
		s->nbItems = 0;
		return 0;
	}
	while (sceIoDread(fd, &dirEntry) > 0)
	{
		if (dirEntry.d_name[0] == '.')
			continue;
		if (dirEntry.d_stat.st_attr & PSP_FILE_TYPE_DIR)
		{
			s->items = realloc(s->items, sizeof(SUBMENUITEM) * (nbFiles + 1));
			memset(&s->items[nbFiles], 0, sizeof(SUBMENUITEM));
			s->items[nbFiles].name = malloc(strlen(dirEntry.d_name) + 2);
			strcpy(s->items[nbFiles].name, dirEntry.d_name);
			strcat(s->items[nbFiles].name, "/");
			s->items[nbFiles].prop1 = dirEntry.d_stat.st_attr;
			s->items[nbFiles].prop2 = EXT_DIR;
			nbFiles++;
			continue;
		}

		ext_id = getExtId(dirEntry.d_name);

		if (ext_id != EXT_UNKNOWN)
		{
			s->items = realloc(s->items, sizeof(SUBMENUITEM) * (nbFiles + 1));
			memset(&s->items[nbFiles], 0, sizeof(SUBMENUITEM));
			s->items[nbFiles].name = malloc(strlen(dirEntry.d_name) + 1);
			strcpy(s->items[nbFiles].name, dirEntry.d_name);
			s->items[nbFiles].prop1 = dirEntry.d_stat.st_attr;
			s->items[nbFiles].prop2 = ext_id;
			nbFiles++;
		}
	}
	sceIoDclose(fd);
	s->nbItems = nbFiles;
	menuFileSelectSortFiles(s, 0);
	return nbFiles;
}

char *getSmallFileName(char *src)		{
	char *ptr = src;
	while (*src)		{
		if ((*src == '/' || *src == '\\') && (*(src+1) != 0))
			ptr = src + 1;
		src++;
	}
	return ptr;
}

int menuFileSelectFillMyPlace(SUBMENU *s, int pathSlot)		{
	int i, j, nbFiles = 0, no_recent = 0;
	freeSubMenuItems(s);

	for (i=0;i<nbPlaces;i++)		{
		j = (i + PATHSLOT_BASE) % nbPlaces;
		//Don't add empty entries
		if (!cPlaces[j][0])
			continue;
		if (j == 0 && no_recent)
			continue;
		if (!strcmp(cPlaces[j], cPlaces[0]))
			no_recent = 1;

		//Add the element
		if (j < PATHSLOT_BASE)		{
			if (j != pathSlot)
				continue;
			const char* recent = "Recent: ";
			const char* smallFileName = getSmallFileName(cPlaces[j]);
			s->items = realloc(s->items, sizeof(SUBMENUITEM) * (nbFiles + 1));
			memset(&s->items[nbFiles], 0, sizeof(SUBMENUITEM));
			s->items[nbFiles].name = malloc(strlen(recent) + strlen(smallFileName) + 1);
			strcpy(s->items[nbFiles].name, recent);
			strcat(s->items[nbFiles].name, smallFileName);
		}
		else
		{
			const char* smallFileName = getSmallFileName(cPlaces[j]);
			s->items = realloc(s->items, sizeof(SUBMENUITEM) * (nbFiles + 1));
			memset(&s->items[nbFiles], 0, sizeof(SUBMENUITEM));
			s->items[nbFiles].name = malloc(strlen(smallFileName) + 1);
			strcpy(s->items[nbFiles].name, smallFileName);
		}
		s->items[nbFiles].prop1 = j;
		if (nPlacesTypes[j] == 1)
			s->items[nbFiles].prop2 = EXT_DIR;
		else if (nPlacesTypes[j] == 2)
			s->items[nbFiles].prop2 = EXT_ZIP;
		nbFiles++;
	}

	s->nbItems = nbFiles;
	return i;
}

void ShowMenuFileSelect(char *savepath, int pathSlot)			{
//	char menuFileSelectPath[256] = "ms0:/PSP/GAME/SMSPlus1.2/ROMS/";

	int i, j, skip = 0, fade, myPlacesFolder = 0;
	SUBMENU *menuFileSelectSubMenu = malloc(sizeof(SUBMENU) * 2);
	menuFileSelectSwap = 0;
	menuFileSelectCurrentMenu = 0;
	
	if (nbPlaces > PATHSLOT_BASE)		{
		strcpy(menuFileSelectPath, "My places");
		myPlacesFolder = 1;
	}
	else if (nbPlaces > pathSlot)		{
		//Don't use NULL strings!
		if (cPlaces[pathSlot][0])
			strcpy(menuFileSelectPath, cPlaces[pathSlot]);
		else
			strcpy(menuFileSelectPath, savepath);
	}
	else
		strcpy(menuFileSelectPath, savepath);

	for (j=0;j<2;j++)		{
		SUBMENU *sm = &menuFileSelectSubMenu[j];
		memset(sm, 0, sizeof(SUBMENU));
		sm->titre = NULL;
		sm->nbItems = 0;
		sm->type = 1;
		//Sélectionné
		if (j == 0)			{
			sm->active = 1;
			sm->moving = 1;
			sm->movingDist = 0;
			sm->movingAlpha = 255;
			sm->movingArrive = 140 * 1;
			sm->close = 0;
		}
		else
			sm->active = -1;
		sm->fctGestion = fctMenuFileSelect;
	}

	if (myPlacesFolder)
		menuFileSelectFillMyPlace(&menuFileSelectSubMenu[menuFileSelectCurrentMenu], pathSlot);
	else
		menuFileSelectFillFiles(&menuFileSelectSubMenu[menuFileSelectCurrentMenu]);
	fade = 0;
	gblFlagLoadParams = 1;
	strcpy(menuFileSelectFileName, "");
	menuSetStatusBarMessageDirect(NULL);

	while(!osl_quit)			{
		//Configure le menu
		LARGEUR_MENU = 480;
		MENU_MAX_DISPLAYED_OPTIONS = 13;

redo:
		MyReadKeys();
		menuStandardVblank();
		if (osl_keys->pressed.square)			{
			osl_keys->pressed.cross = 1;
			gblFlagLoadParams = 0;
		}
		//Rond ==> annnuler
		if (osl_keys->pressed.circle)			{
			strcpy(menuFileSelectFileName, "");
			osl_keys->pressed.circle = 0;
			break;
		}
		//Triangle ==> annuler
		if (osl_keys->pressed.triangle && !myPlacesFolder)			{
			osl_keys->pressed.triangle = 0;
			osl_keys->pressed.circle = 1;
		}
		//Start ==> Options
		if (osl_keys->pressed.start)		{
			SUBMENU *s = &menuFileSelectSubMenu[menuFileSelectCurrentMenu];
			char *contents_normal[] = {"Add to my places", "Delete ROM", "Delete SRAM", "Delete states", "Delete config"};
			char *contents_folder[] = {"Add to my places"};
			char *contents_myplaces[] = {"Remove from my places"};
			int result;
			if (myPlacesFolder)		{
				result = ShowChoiceMsgBox("What do you want to do?", "Options", MB_OKCANCEL, 200, contents_myplaces, oslNumberof(contents_myplaces), 1);
				if (result == 1)		{
					RemoveMyPlacesEntry(s->items[s->state.selection].prop1);
					if (s->state.selection < s->nbItems - 1)
						memmove(&s->items[s->state.selection], &s->items[s->state.selection+1], sizeof(s->items[s->state.selection]) * (s->nbItems - s->state.selection - 1));
					s->nbItems--;
					for (i=s->state.selection;i<s->nbItems;i++)		{
						//Do not change the "recent" element (0) else we'll be able to delete it!
						if (s->items[i].prop1 >= PATHSLOT_BASE)
							s->items[i].prop1--;
					}
					if (s->state.selection >= s->nbItems)
						s->state.selection--;
				}
			}
			else			{
				char temp[MAX_PATH];
				char name[MAX_PATH];
				int removeAll = 0;
				strcpy(temp, menuFileSelectPath);
				strcat(temp, s->items[s->state.selection].name);
				if (s->items[s->state.selection].prop2 == EXT_DIR)
					result = ShowChoiceMsgBox("What do you want to do?", "Options", MB_OKCANCEL, 200, contents_folder, oslNumberof(contents_folder), 1);
				else
					result = ShowChoiceMsgBox("What do you want to do?", "Options", MB_OKCANCEL, 200, contents_normal, oslNumberof(contents_normal), 1);
				//Add to my places
				if (result == 1)		{
					if (s->items[s->state.selection].prop2 == EXT_DIR)
						AddToMyPlaces(temp, 1);
					else
						AddToMyPlaces(temp, 2);
				}
				//Delete ROM
				if (result == 2)		{
					if (MessageBoxEx("Do you want to also remove any save data and settings for this game?", "Question", MB_YESNO, 220))
						removeAll = 1;
					if (MessageBoxEx("The ROM will definately be removed! Are you sure?", "Question", MB_YESNO, 220))		{
						sceIoRemove(temp);
						if (s->state.selection < s->nbItems - 1)
							memmove(&s->items[s->state.selection], &s->items[s->state.selection+1], sizeof(s->items[s->state.selection]) * (s->nbItems - s->state.selection - 1));
						s->nbItems--;
						if (s->state.selection >= s->nbItems)
							s->state.selection--;
					}
					else
						removeAll = 0;
				}
				//Delete SRAM
				if (result == 3 || removeAll)		{
					if (!removeAll)
						result = MessageBoxEx("The SRAM file for this game will definately be removed! Are you sure?", "Question", MB_YESNO, 220);
					if (result)		{
						pspGetStateNameEx(temp, name, STATE_SRAM);
						sceIoRemove(name);
					}
				}
				//Delete states
				if (result == 4 || removeAll)		{
					if (!removeAll)
						result = MessageBoxEx("ALL save states for this game will be definately deleted!!! Are you sure?", "Question", MB_YESNO, 220);
					if (result)		{
						for (i=0;i<=11;i++)		{
							if (i != STATE_RAM)		{
								pspGetStateNameEx(temp, name, i);
								sceIoRemove(name);
							}
						}
					}
				}
				//Delete config
				if (result == 5 || removeAll)		{
					if (result)		{
						pspGetStateNameEx(temp, name, STATE_CONFIG);
						sceIoRemove(name);
/*						char *ptr = strrchr(temp, '.');
						if (ptr)			{
							strcpy(ptr, ".ini");
							sceIoRemove(temp);
						}*/
					}
				}
			}

			//We can't use a menu which has got less than 1 option
			if (s->nbItems <= 0)
				break;
		}

		if (menuFileSelectSubMenu[menuFileSelectCurrentMenu].nbItems == 0)
			menuFileSelectSwap = -1;

		fade = min(fade + 16, 255);

		if (menuFileSelectSwap)		{
			SUBMENU *s;
			char temp[MAX_PATH];

			strcpy(temp, menuFileSelectPath);
			//Croix (valider)
			if (menuFileSelectSwap == 1)		{
				s = &menuFileSelectSubMenu[menuFileSelectCurrentMenu];
				if (s->items[s->state.selection].prop2 == EXT_DIR)			{
					if (myPlacesFolder)
						strcpy(menuFileSelectPath, cPlaces[s->items[s->state.selection].prop1]);
					else
						strcat(menuFileSelectPath, s->items[s->state.selection].name);
				}
				else	{
					//Lancer la ROM
					if (!myPlacesFolder)		{
						strcpy(menuFileSelectFileName, menuFileSelectPath);
						strcat(menuFileSelectFileName, s->items[s->state.selection].name);
					}
					else
						strcpy(menuFileSelectFileName, cPlaces[s->items[s->state.selection].prop1]);

					//Eventually remove the first / in the path
					if (*menuFileSelectFileName == '/')
						memmove(menuFileSelectFileName, menuFileSelectFileName + 1, strlen(menuFileSelectFileName));

					//Sauve le chemin
					if (!myPlacesFolder)		{
						if (strcmp(savepath, menuFileSelectPath))		{
							strcpy(savepath, menuFileSelectPath);

							//If the current entry doesn't exist in my places, we set it as the last folder.
							int i;
							for (i=PATHSLOT_BASE;i<nbPlaces;i++)		{
								if (!strcmp(savepath, cPlaces[i]))
									break;
							}
							//Not found? => add it.
							if (i >= nbPlaces)
								SetMyPlacesEntry(pathSlot, savepath, 1);
						}
					}
					break;
				}
			}
			else	{
				//Annuler => répertoire parent
				j = -1;
				for (i=0;menuFileSelectPath[i];i++)		{
					//Dernier slash (mais pas en dernier caractère)
					if ((menuFileSelectPath[i] == '/' || menuFileSelectPath[i] == '\\') && menuFileSelectPath[i + 1])
						j = i;
				}
				if (j >= 0)
					menuFileSelectPath[j + 1] = 0;
			}

			menuFileSelectCurrentMenu = 1 - menuFileSelectCurrentMenu;
			if (!menuFileSelectFillFiles(&menuFileSelectSubMenu[menuFileSelectCurrentMenu]))			{
				//On était en retour arrière!!
				if (menuFileSelectSwap == -1)
					goto redo;
				MessageBox("The selected folder is empty.", "Information", MB_OK);
				strcpy(menuFileSelectPath, temp);
				menuFileSelectCurrentMenu = 1 - menuFileSelectCurrentMenu;
			}
			else		{
				//We are entering in another folder and leaving my places.
				myPlacesFolder = 0;

				s = &menuFileSelectSubMenu[menuFileSelectCurrentMenu];
				s->active = 1;
				s->moving = menuFileSelectSwap;
				s->movingDist = 0;
				s->movingAlpha = 255;
				s->movingArrive = 140 * s->moving;
				s->close = 0;

				s->state.selection = 0;
				s->state.selectionDirect = 1;
				//Retour en arrière
				if (menuFileSelectSwap == -1)		{
					//Trouve le dossier qui était sélectionné
					for (i=0;i<s->nbItems;i++)		{
						if (!strcmp(s->items[i].name, temp + j + 1))		{
							s->state.selection = i;
						}
					}
				}

				//Trouve le plus petit enfant de l'ancienne option...
				s = &menuFileSelectSubMenu[1 - menuFileSelectCurrentMenu];
				while (s->child)		{
					s->close = 1;
					s->active = 2;
					s = s->child;
				}
				s->active = 2;
				s->moving = menuFileSelectSwap;
				s->movingDist = 0;
				s->movingAlpha = 255;
				s->movingArrive = 140 * s->moving;
				s->close = 1;
			}

			menuFileSelectSwap = 0;
		}

		HandleSubMenu(&menuFileSelectSubMenu[0]);
		HandleSubMenu(&menuFileSelectSubMenu[1]);
		HandleBackground();

		if (!skip)		{
			menuSetStatusBarMessageDirect("\x13: Run  \x14: Run default  \x11: Parent  \x12: Cancel  Start: Options");

			oslStartDrawing();
			DrawBackground();

//			oslPrintf_xy(0, 0, "%i", menuFileSelectSubMenu[0].state.active);
			oslSetFont(ftGlow);
			MySetTextColor((menuColorSubmenuTitle & 0xffffff) | (fade << 24));
			oslDrawString(30, 27, menuFileSelectPath);
			oslSetTextColor(RGB(255, 255, 255));
			oslSetFont(ftStandard);
			DrawSubMenu(&menuFileSelectSubMenu[0], 30, 55, TRUE);
			DrawSubMenu(&menuFileSelectSubMenu[1], 30, 55, TRUE);

			DrawBackgroundAfter();

			oslEndDrawing();
		}
		skip = oslSyncFrame();
	}

	freeSubMenuItems(&menuFileSelectSubMenu[0]);
	freeSubMenuItems(&menuFileSelectSubMenu[1]);
	free(menuFileSelectSubMenu);
	menuSetStatusBarMessageDirect(NULL);
}

STEXTENSIONLIST *stExtentions;

STEXTENSIONLIST stRomExtentions[] = 
{
	"gb" , EXT_GB,
	"gbc", EXT_GB,
	"sgb", EXT_GB,
	// "pce", EXT_PCE ,
	"zip", EXT_ZIP ,
//	"gz" , EXT_ZIP ,
//	"tgz", EXT_ZIP ,
	"sms", EXT_SMS,
	"gg",  EXT_GG,
	NULL , EXT_UNKNOWN
};

STEXTENSIONLIST stMusicExtentions[] = 
{
	// "gb" , EXT_GB  ,
	// "gbc", EXT_GB  ,
	// "pce", EXT_PCE ,
	"zip", EXT_ZIP ,
	NULL , EXT_UNKNOWN
};

int fctMsgBoxChoice(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	if (event == EVENT_SET)		{
	}
	else if (event == EVENT_SELECT)			{
		if (!sub->disabled)		{
			gblMsgBoxChoiceValue = sub->prop1 + 1;
			CloseWindow(menuCurrentWindow, 1);
		}
		else
			return 0;
	}
	return 1;
}

void fctMsgBoxChoice_Close(WINDOW *w, int valid)		{
	//User cancelled
	if (!valid)
		gblMsgBoxChoiceValue = 0;
}

void fctMsgBoxChoice_Handle(WINDOW *w, int event)		{
	if (event == EVENT_DRAW)		{
		int xx0, xx1, yy0, yy1, lm = LARGEUR_MENU;
		xx0 = (LARG - w->largeur) / 2;
		xx1 = xx0 + w->largeur;
		yy0 = (HAUT - w->hauteur) / 2;
		yy1 = yy0 + w->hauteur;
		LARGEUR_MENU = xx1 - xx0 - 26;
		DrawSubMenu((SUBMENU*)w->userData[0], xx0 + 4, yy0 + 37, w->multiply >= MSGBOX_MAX_MULTIPLY);
		LARGEUR_MENU = lm;
	}
	else if (event == EVENT_HANDLE)		{
		HandleSubMenu((SUBMENU*)w->userData[0]);
		//Ne jamais laisser gérer ça par la boîte sinon elle se fermera!
		osl_keys->pressed.cross = 0;
	}
}

void fctMsgBoxChoice_Destroy(WINDOW *w)		{
	SUBMENU *s = (SUBMENU*)w->userData[0];
	if (s)		{
		if (s->items)
			free(s->items);
			s->items = NULL;
	}
}

int ShowChoiceMsgBoxEx(char *msg, char *title, int buttons, int width, SUBMENU *sm, int blocking)		{
	gblMsgBoxChoiceValue = -1;
	MessageBoxAsync(msg, title, buttons, width, fctMsgBoxChoice_Close, fctMsgBoxChoice_Handle);
	//Don't show more than 5 items at once (default)
	menuCurrentWindow->hauteur += 10 + HAUTEUR_TEXTE * oslMin(sm->nbItems, 5);
	menuCurrentWindow->userData[0] = (u32)sm;
	menuCurrentWindow->fctDestroy = fctMsgBoxChoice_Destroy;
	SelectSubMenu(sm, 1);
	sm->active = 1;
	sm->moving = 0;
	sm->close = 0;
	if (blocking)		{
		OSL_IMAGE *imgFond;
		WINDOW *w = menuCurrentWindow;
		int skip = 0;
		//Copie le backbuffer sur une image
		oslLockImage(OSL_SECONDARY_BUFFER);
		imgFond = oslCreateSwizzledImage(OSL_SECONDARY_BUFFER, OSL_IN_RAM);
		oslUnlockImage(OSL_SECONDARY_BUFFER);
		if (menuIsInGame == 2)
			SoundPause();
		while(!osl_quit && gblMsgBoxChoiceValue == -1)			{
			MyReadKeys();
			menuStandardVblank();
			//Pas de bouton => direct au max
			if (w->buttons == MB_NONE)
				w->multiply = MSGBOX_MAX_MULTIPLY;
			HandleWindow(w);
			//Dessine une fois pour MB_NONE
			if (!skip)		{
				oslStartDrawing();
				sceGuDisable(GU_DITHER);
				oslDrawImage(imgFond);
				sceGuEnable(GU_DITHER);
				DrawWindow(w);
				oslEndDrawing();
			}
			skip = oslSyncFrame();
			menuFrameNb++;
			frameCount++;
		}
		oslDeleteImage(imgFond);
		if (menuIsInGame == 2)
			SoundResume();
	}
	return gblMsgBoxChoiceValue;
}

int ShowChoiceMsgBox(char *msg, char *title, int buttons, int width, char *elements[], int nbElements, int blocking)		{
	int i;

	memset(&choiceMsgBoxMenu, 0, sizeof(choiceMsgBoxMenu));

	//Has not been freed since the last time - should not happen
	if (choiceMsgBoxMenu.items)
		free(choiceMsgBoxMenu.items);
	choiceMsgBoxMenu.items = malloc(sizeof(SUBMENUITEM) * nbElements);

	if (!choiceMsgBoxMenu.items)
		return;

	memset(choiceMsgBoxMenu.items, 0, sizeof(SUBMENUITEM) * nbElements);
	for (i=0;i<nbElements;i++)		{
		choiceMsgBoxMenu.items[i].name = elements[i];
		choiceMsgBoxMenu.items[i].description = "";
		choiceMsgBoxMenu.items[i].prop1 = i;
	}

	choiceMsgBoxMenu.nbItems = nbElements;
	choiceMsgBoxMenu.type = 1;
	choiceMsgBoxMenu.fctGestion = fctMsgBoxChoice;
	return ShowChoiceMsgBoxEx(msg, title, buttons, width, &choiceMsgBoxMenu, blocking);
}


int fctPaletteList(SUBMENU *menu, SUBMENUITEM *sub, u32 event)
{
	if (event == EVENT_SET)		{
	}
	else if (event == EVENT_SELECT)			{
		if (sub->prop1 == 0)		{
			//[Default] entry
			menuConfig.gameboy.palette[0] = 0;
		}
		else
			strcpy(menuConfig.gameboy.palette, sub->name);
		UpdateMenus(MENUUPD_VIDEO);
		CloseWindow(menuCurrentWindow, 1);
	}
	return 1;
}

SUBMENU menuPaletteList=		{
	NULL, 0,
	1,			//Menu
	NULL,
	fctPaletteList
};

void menuShowPaletteList()		{
	ScriptAjouteProcedures("palettes.ini", &menuPaletteList, "[Default]");
	if (menuPaletteList.nbItems > 0)
		ShowChoiceMsgBoxEx("Please select a palette:", "Palettes", MB_OKCANCEL, 250, &menuPaletteList, 0);
}

