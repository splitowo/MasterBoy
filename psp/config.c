//Brunni's portable config system (qui déchire trop sa race... hem en fait non)
#include "pspcommon.h"

MENUPARAMS *config_referenceConfig = NULL;
int retourScript;
int arretdurgence=0;
char *trouveLabel=NULL, chneLabel[1000];
enum types_objets {TYPE_FONCTION, TYPE_ENTIER, TYPE_BOOL, TYPE_CHAINE, TYPE_REEL, TYPE_OBJET, TYPE_TABLEAU};
//Indique que ce n'est pas sauvé automatiquement
#define FLAG_NOSAVE 0x1
//Pas encore utilisé
#define FLAG_CALLBACK 0x2
//Pas sauver pour les fichiers de config des jeux
#define FLAG_NOSAVEGAME 0x4

//Octets utilisés pour les types
#define MASQUE_TYPE 15
//Taille max (0 à 15: 0 = 1, 1 = 2, 2 = 3, 4 = 16, etc.)
#define TYPE_MAXSIZE(x)				((x) << 4)
#define TYPE_MAXSIZE_MASK(flags)	((flags >> 4) & 15)
#define ScriptErrorEx(format...)	({ char __str[1000]; sprintf(__str , ##format); ScriptError(__str); })

static void ScriptError(char *msg);

VIRTUAL_FILE *gblFichierConfig;
int bModeWideCrLf=1;
int gblIndice;
char gblVarName[TAILLE_MAX_CHAINES];
enum {FCT_SET=1, FCT_GET, FCT_SET_BEFORE, FCT_SET_AFTER};
char tempValue[TAILLE_MAX_CHAINES];
char menuDefaultExecute[TAILLE_MAX_CHAINES];
int GetEntier(char *valeur);
char *ProchArgEntier(char *ptr);
char *GetChaine(char **dest);
#define SAUTE_ESPACES(ptr)	 { while(*ptr==' ' || *ptr=='	') ptr++; }

char *FctFin(int type, char *ptr)		{
	arretdurgence=1;
	return NULL;
}

char *FctGoto(int type, char *ptr)		{
	//Note: les labels ne peuvent être qu'en dessous
	char *nom = GetChaine(&ptr);
	if (nom)		{
		strcpy(chneLabel, nom);
		trouveLabel = chneLabel;
	}
	return NULL;
}

char *FctFaire(int type, char *ptr)			{
	char *nom, *fct;
	nom=GetChaine(&ptr);
	while(*ptr!=',' && *ptr!=':' && *ptr)
		ptr++;
	if (*ptr)
		ptr++;
	while(*ptr!='\"' && *ptr)
		ptr++;
	fct = GetChaine(&ptr);
	if (nom)			{
		if (!OuvreFichierConfig(nom, fct))			{
			ScriptErrorEx("Unable to execute the specified file: %s", nom);
		}
	}
	else
		ScriptError("Function: Do\nFilename not defined correctly (don't forget the quote marks)");
	return NULL;
}

char *FctMsgBox(int type, char *ptr)		{
	char *chne, *label, *tmp, *titre;

	chne=GetChaine(&ptr);
	if (!chne)
		chne = "Breakpoint defined here (MsgBox without argument).";

	titre = "User message";

prochArg:
	if (*ptr==',')
		ptr++;
	while(*ptr==' ' || *ptr=='	')		ptr++;

	tmp=GetChaine(&ptr);

	label=NULL;

	if (!tmp)		{
		if (!strncmp(ptr, "else goto ", 10))
			label=ptr+10;
	}
	else		{
		titre=tmp;
//		goto prochArg;
	}

	if (MessageBox(chne, titre, MB_OKCANCEL) == 0)		{
		if (label)		{
			strcpy(chneLabel,label);
			trouveLabel=chneLabel;
		}
		else
			arretdurgence=1;
	}
	return NULL;
}

char *FctPalette(int type, char *ptr)			{
	OSL_COLOR palCol[12];
	int i = 0;

	//Max. 12 couleurs
	do 		{
		palCol[i++] = GetEntier(ptr) | 0xff000000;
		ptr = ProchArgEntier(ptr);
	} while (i < 12 && *ptr);

	//4 couleurs: version compacte
	if (i == 4)		{
		memcpy(palCol + 4, palCol, 4 * sizeof(*palCol));
		memcpy(palCol + 8, palCol, 4 * sizeof(*palCol));
		i = 12;
	}

	if (i == 12)		{
		//Copie dans le cache
		memcpy(currentPalCache, palCol, sizeof(palCol));
		//Invalide
		gb_invalidate_all_colors();
	}

	//Pour ne pas avoir besoin de faire un end à la fin
	arretdurgence = 1;
	return NULL;
}

u32 GetAddedColor(int color, int value)		{
	return min(max((color & 0xff) + value, 0), 255) |
		(min(max(((color & 0xff00) >> 8) + value, 0), 255) << 8) |
		(min(max(((color & 0xff0000) >> 16) + value, 0), 255) << 16);
}

char *FctTabMenuColCursor(int type, char *ptr)		{
	//Erreur => on doit le faire nous même...
	if (type == FCT_SET)		{
		int valeur;
		valeur = GetEntier(ptr);
		menuColorCursor[0] = GetAddedColor(valeur, 16);
		menuColorCursor[1] = GetAddedColor(valeur, 32);
		menuColorCursor[2] = GetAddedColor(valeur, -48);
		menuColorCursor[3] = GetAddedColor(valeur, -32);
	}
	if (type == FCT_SET_AFTER)		{
		int i;
		for (i=0;i<4;i++)
			menuColorCursor[i] &= 0xffffff;
	}
	return NULL;
}

char *FctMusicFilename(int type, char *ptr)		{
	//Erreur => on doit le faire nous même...
	if (type == FCT_SET_AFTER)		{
		if (musicEnabled)
			menuStartSound(menuConfig.music.filename);
	}
	return NULL;
}

extern byte tilePalList[384];
extern short tilePalPalette[NB_PALETTES][4];
extern byte tilePalCrc[384 / 8];
//extern byte tileMapAltVram[384 * 16];
extern short tileMapTileList[384];

void FctColorIt_SetPalette(int type, char *ptr)		{
	int palNo = GetEntier(ptr), color, i;
	if (palNo < 0 || palNo >= NB_PALETTES)
		return;
	for (i=0;i<4;i++)			{
		ptr = ProchArgEntier(ptr);
		if (*ptr)		{
			color = GetEntier(ptr) | 0xff000000;
			tilePalPalette[palNo][i] = oslConvertColor(OSL_PF_5551, OSL_PF_8888, color);
		}
	}
	gb_invalidate_palette(palNo);
}

void FctColorIt_AddTileRule(int type, char *ptr)		{
	int tileStart = 0, tileEnd = 0, palNo, i;

	//Récupère les arguments
	if (*ptr == '+')	{
		tileStart = 128;
		ptr++;
	}
	tileStart += GetEntier(ptr);
	ptr = ProchArgEntier(ptr);
	//+tileno signifie que c'est une tile dans la zone haute de la VRAM (+128)
	if (*ptr == '+')	{
		tileEnd = 128;
		ptr++;
	}
	tileEnd += GetEntier(ptr);
	ptr = ProchArgEntier(ptr);
	if (*ptr)
		palNo = GetEntier(ptr);
	else	{
		//Only two args => the second is the palette number
		palNo = tileEnd;
		tileEnd = tileStart;
	}

	//Vérifie que les valeurs fournies sont correctes
	if (tileStart < 0 || tileStart >= 384 || tileEnd < 0 || tileEnd >= 384 || palNo < 0 || palNo >= NB_PALETTES)
		return;

	//Définit les palettes des tiles
	for (i=tileStart;i<=tileEnd;i++)
		tilePalList[i] = palNo;
}

void FctColorIt_AddTileCrc(int type, char *ptr)		{
	int tileStart = 0, tileEnd = 0;
	//Récupère les arguments
	if (*ptr == '+')	{
		tileStart = 128;
		ptr++;
	}
	tileStart += GetEntier(ptr);
	ptr = ProchArgEntier(ptr);
	//+tileno signifie que c'est une tile dans la zone haute de la VRAM (+128)
	if (*ptr == '+')	{
		tileEnd = 128;
		ptr++;
	}
	tileEnd += GetEntier(ptr);
	if (tileStart < 0 || tileStart >= 384 || tileEnd < 0 || tileEnd >= 384)
		return;
	//Savoir si une tile a son crc compté:
	//(tilePalCrc[tileNo >> 8] & (1 << (tileNo & 7)))
	for (;tileStart<=tileEnd;tileStart++)
		tilePalCrc[tileStart >> 3] |= 1 << (tileStart & 7);
}


void FctColorIt_SetTilesetData(int type, char *ptr)		{
	int tileNo = 0, i, j;
	char *text;
	char mode = 'N';
	u8 *tilePtr;

	//Récupère les arguments
	if (*ptr == '+')	{
		tileNo = 128;
		ptr++;
	}
	tileNo += GetEntier(ptr);

	if (tileNo < 0 || tileNo >= 384)
		return;

	ptr = ProchArgEntier(ptr);
	if (*ptr == 'l')		{
		ptr++;
		mode = 'L';
		while (*ptr == ' ' || *ptr == '	')
			ptr++;
	}
	text = GetChaine(&ptr);
	if (!text)
		return;

	//We have two VRAM banks just like the game boy color, the second one being used for the custom tileset.
	tileNo += 512;
	tilePtr = (u8*)vram + (tileNo << 4);

	//Mode normal: données RAW au format GB
	if (mode == 'N')		{
		//16 bytes per tile
		for (i=0;i<16;i++)		{
			//Récupération d'un nombre (doublet) hexadécimal
			u8 value1, value2;
			do	{
				value1 = *text++;
			} while (value1 == ' ');
			if (!value1)
				break;
			do	{
				value2 = *text++;
			} while (value2 == ' ');
			if (!value2)
				break;
			if (value1 >= 'a' && value1 <= 'f')
				value1 -= 'a' - ('9' + 1);
			if (value2 >= 'a' && value2 <= 'f')
				value2 -= 'a' - ('9' + 1);
			value1 -= '0';		
			value2 -= '0';

			//Store the final value
			*tilePtr++ = value2 + (value1 << 4);
		}
	}
	else if (mode == 'L')		{
		//8*8 pixels per tile
		for (j=0;j<8;j++)		{
			for (i=7;i>=0;i--)		{
				//Récupération d'un nombre
				u8 value;
				do	{
					value = *text++;
				} while (value == ' ');

				//Affectation des bits selon la valeur (1 à 4)
				if (value == '0' || value == '3')
					tilePtr[0] |= 1 << i;
				else
					tilePtr[0] &= ~(1 << i);

				if (value == '2' || value == '3')
					tilePtr[1] |= 1 << i;
				else
					tilePtr[1] &= ~(1 << i);
			}

			//2 bytes per line
			tilePtr += 2;
		}
	}
}

void FctColorIt_SetTile(int type, char *ptr)		{
	int tileStart = 0, tileEnd = 0, newTile = 0;
	int redo = 2;

	//Récupère les arguments
	if (*ptr == '+')	{
		tileStart = 128;
		ptr++;
	}
	tileStart += GetEntier(ptr);
	ptr = ProchArgEntier(ptr);

	tileEnd = tileStart;
	while (redo--)		{
		//Récupère les arguments
		if (*ptr == 'o')						//o like original (original tileset)
			ptr++, newTile = 0;
		else
			newTile = 512;
		if (*ptr == '+')	{
			newTile += 128;
			ptr++;
		}
		newTile += GetEntier(ptr);
		ptr = ProchArgEntier(ptr);

		//Un troisième argument?
		if (*ptr)
			//Ne pas prendre si 512 a été ajouté car ça n'est valable que pour newTile
			tileEnd = newTile & 511;
		else
			redo = 0;
	}

	if (tileStart < 0 || tileStart >= 384 || tileEnd < 0 || tileEnd >= 384 || newTile < 0 || newTile >= 384 + 512)
		return;

	for (;tileStart<=tileEnd;)
		tileMapTileList[tileStart++] = newTile++;
}

char *FctImgBack(int type, char *ptr);
char *FctImgIcons(int type, char *ptr);
char *FctImgNumbers(int type, char *ptr);
char *FctFtStandard(int type, char *ptr);
char *FctFtGlow(int type, char *ptr);

const IDENTIFICATEUR objFile[]=	{
	{"path",				gblRomPath,							TYPE_CHAINE | TYPE_MAXSIZE(8), FLAG_NOSAVE},
	{"defaultExecute",		menuDefaultExecute,					TYPE_CHAINE | TYPE_MAXSIZE(8), FLAG_NOSAVEGAME},
	{"state",				&menuConfig.file.state,				TYPE_ENTIER},
	{"sramAutosave",		&menuConfig.file.sramAutosave,		TYPE_BOOL},
};

const IDENTIFICATEUR objVideo[]=	{
	{"country",				&menuConfig.video.country,			TYPE_ENTIER},
	{"render",				&menuConfig.video.render,			TYPE_ENTIER},
	{"smoothing",			&menuConfig.video.smoothing,		TYPE_BOOL},
	{"frameskip",			&menuConfig.video.frameskip,		TYPE_ENTIER},
	{"frameskipMax",		&menuConfig.video.frameskipMax,		TYPE_ENTIER},
	{"vsync",				&menuConfig.video.vsync,			TYPE_ENTIER},
	{"turboSkip",			&menuConfig.video.turboSkip,		TYPE_ENTIER},
	{"background",			&menuConfig.video.background,		TYPE_ENTIER},
	{"brightnessMode",		&menuConfig.video.brightnessMode,	TYPE_ENTIER},
	{"gamma",				&menuConfig.video.gamma,			TYPE_ENTIER},
	{"vibrance",			&menuConfig.video.vibrance,			TYPE_ENTIER},
	{"renderDepth",			&menuConfig.video.renderDepth,		TYPE_ENTIER},
	{"cpuTime",				&menuConfig.video.cpuTime,			TYPE_ENTIER},
	{"leftBar",				&menuConfig.video.leftBar,			TYPE_ENTIER},
	{"spriteLimit",			&menuConfig.video.spriteLimit,		TYPE_BOOL},
	{"syncMode",			&menuConfig.video.syncMode,			TYPE_ENTIER},
};

const IDENTIFICATEUR objSound[]=	{
	{"enabled",				&menuConfig.sound.enabled,			TYPE_BOOL},
	{"sampleRate",			&menuConfig.sound.sampleRate,		TYPE_ENTIER},
	{"stereo",				&menuConfig.sound.stereo,			TYPE_ENTIER},
	{"volume",				&menuConfig.sound.volume,			TYPE_ENTIER},
	{"turboSound",			&menuConfig.sound.turboSound,		TYPE_ENTIER},
	{"syncMode",			&menuConfig.sound.perfectSynchro,	TYPE_ENTIER},
};

INFOS_TABLEAU infoTabCtrlAnalogCalValues[] = {4, TYPE_ENTIER, menuConfig.ctrl.analog.calValues};

const IDENTIFICATEUR objCtrlAnalog[]=	{
	{"todPad",				&menuConfig.ctrl.analog.toPad,		TYPE_BOOL},
	{"treshold",			&menuConfig.ctrl.analog.treshold,	TYPE_ENTIER},
	{"useCalibration",		&menuConfig.ctrl.analog.useCalibration,	TYPE_BOOL},
	{"calValues",			infoTabCtrlAnalogCalValues,			TYPE_TABLEAU, FLAG_NOSAVEGAME},
};

INFOS_OBJET infoObjCtrlAnalog[]={numberof(objCtrlAnalog), objCtrlAnalog};
INFOS_TABLEAU infoTabCtrlKeys[] = {NBRE_TOUCHES, TYPE_ENTIER, menuConfig.ctrl.akeys};
INFOS_TABLEAU infoTabCtrlCuts[] = {NBRE_CUTS, TYPE_ENTIER, menuConfig.ctrl.acuts};

const IDENTIFICATEUR objCtrl[]=	{
	{"analog",				infoObjCtrlAnalog,					TYPE_OBJET},
	{"keys",				infoTabCtrlKeys,					TYPE_TABLEAU},
	{"shortcuts",			infoTabCtrlCuts,					TYPE_TABLEAU},
	{"autofireRate",		&menuConfig.ctrl.autofireRate,		TYPE_ENTIER},
};

const IDENTIFICATEUR objMisc[]=	{
	{"pspClock",			&menuConfig.misc.pspClock,			TYPE_ENTIER},
	{"z80Clock",			&menuConfig.misc.z80Clock,			TYPE_ENTIER},
};

INFOS_TABLEAU infoTabMenuColCursor[] = {4, TYPE_ENTIER, &menuColorCursor};

const IDENTIFICATEUR objMenu[]=	{
	{"imgBack",				FctImgBack,							TYPE_FONCTION},
	{"imgIcons",			FctImgIcons,						TYPE_FONCTION},
	{"imgNumbers",			FctImgNumbers,						TYPE_FONCTION},
	{"ftStandard",			FctFtStandard,						TYPE_FONCTION},
	{"ftGlow",				FctFtGlow,							TYPE_FONCTION},
	{"colCursor",			infoTabMenuColCursor,				TYPE_TABLEAU, FLAG_CALLBACK, FctTabMenuColCursor},
	{"colSubmenuTitle",		&menuColorSubmenuTitle,				TYPE_ENTIER},
	{"animateCursor",		&menuAnimateCursor,					TYPE_BOOL},
};

const IDENTIFICATEUR objMusic[]=	{
	{"path",				gblMusicPath,						TYPE_CHAINE | TYPE_MAXSIZE(8), FLAG_NOSAVE},
	{"file",				menuConfig.music.filename,			TYPE_CHAINE | TYPE_MAXSIZE(8), FLAG_NOSAVEGAME|FLAG_CALLBACK, FctMusicFilename},
	{"sampleRate",			&menuConfig.music.frequency,		TYPE_ENTIER, FLAG_NOSAVEGAME},
	{"repeatMode",			&menuConfig.music.repeatMode,		TYPE_ENTIER, FLAG_NOSAVEGAME},
};

const IDENTIFICATEUR objGameBoy[]=	{
	{"colorization",		&menuConfig.gameboy.colorization,	TYPE_ENTIER, FLAG_NOSAVEGAME},
	{"palette",				&menuConfig.gameboy.palette,		TYPE_CHAINE},
	{"gbType",				&menuConfig.gameboy.gbType,			TYPE_ENTIER},
};

const IDENTIFICATEUR objColorIt[]=	{
	//ColorIt.setPalette palNo, palColor0, palColor1, palColor2, palColor3
	{"setPalette",			FctColorIt_SetPalette,				TYPE_FONCTION},
	//ColorIt.addTileRule tileStart, tileEnd, palNo
	{"addTileRule",			FctColorIt_AddTileRule,				TYPE_FONCTION},
	//ColorIt.addTileCrc tileNo
	{"addTileCrc",			FctColorIt_AddTileCrc,				TYPE_FONCTION},
	//ColorIt.setTileData tileNo, "hextiledata"
	{"setTilesetData",		FctColorIt_SetTilesetData,			TYPE_FONCTION},
	//ColorIt.setTile tileNo, newTileNo
	{"setTile",				FctColorIt_SetTile,					TYPE_FONCTION},
	//Automatically displays the VRAM CRC when the VRAM changes
	{"autoShowVramCrc",		&gblConfigAutoShowCrc,				TYPE_BOOL},
};

INFOS_OBJET infoObjFile[]={numberof(objFile), objFile};
INFOS_OBJET infoObjVideo[]={numberof(objVideo), objVideo};
INFOS_OBJET infoObjSound[]={numberof(objSound), objSound};
INFOS_OBJET infoObjCtrl[]={numberof(objCtrl), objCtrl};
INFOS_OBJET infoObjMisc[]={numberof(objMisc), objMisc};
INFOS_OBJET infoObjMenu[]={numberof(objMenu), objMenu};
INFOS_OBJET infoObjMusic[]={numberof(objMusic), objMusic};
INFOS_OBJET infoObjGameBoy[]={numberof(objGameBoy), objGameBoy};
//INFOS_OBJET infoObjGameboy[]={numberof(objGameboy), objGameboy};
INFOS_OBJET infoObjColorIt[]={numberof(objColorIt), objColorIt};

const IDENTIFICATEUR objets[]=		{
	{"file",				infoObjFile,			TYPE_OBJET},
	{"video",				infoObjVideo,			TYPE_OBJET},
	{"sound",				infoObjSound,			TYPE_OBJET},
	{"ctrl",				infoObjCtrl,			TYPE_OBJET},
	{"misc",				infoObjMisc,			TYPE_OBJET},
	{"menu",				infoObjMenu,			TYPE_OBJET, FLAG_NOSAVE},
	{"music",				infoObjMusic,			TYPE_OBJET},
	{"gameboy",				infoObjGameBoy,			TYPE_OBJET},
	{"colorit",				infoObjColorIt,			TYPE_OBJET, FLAG_NOSAVE},

	//Standard du langage
	{"palette",				FctPalette,				TYPE_FONCTION, FLAG_NOSAVE},
	{"end",					FctFin,					TYPE_FONCTION, FLAG_NOSAVE},
	{"msgbox",				FctMsgBox,				TYPE_FONCTION, FLAG_NOSAVE},
	{"goto",				FctGoto,				TYPE_FONCTION, FLAG_NOSAVE},
	{"do",					FctFaire,				TYPE_FONCTION, FLAG_NOSAVE},
//	{"quit",				FctQuitte,				TYPE_FONCTION, FLAG_NOSAVE},
//	{"exec",				FctExec,				TYPE_FONCTION, FLAG_NOSAVE},
};

//Un objet qui les contient tous
INFOS_OBJET infoObjMain[]={numberof(objets), objets};

//Comparaison entre une chaîne à casse basse et mixte
int strlwrmixcmp(const char *lower, const char *mix)			{
	while(*lower || *mix)		{
		char c = *mix;
		if (c >= 'A' && c <= 'Z')
			c += 32;
		if (c != *lower)
			return -1;
		lower++, mix++;
	}
	return 0;
}

//Returns the corresponding address in config_referenceConfig from a member of menuConfig
static void *getOffsetedAddr(void *value)		{
	if (!config_referenceConfig)
		return NULL;
	if ((u8*)value >= (u8*)(&menuConfig) && (u8*)value < (u8*)(&menuConfig) + sizeof(menuConfig))			{
		u32 offset = (u8*)value - (u8*)(&menuConfig);
		void *adr = (void*)((u32)(config_referenceConfig) + offset);
		return adr;
	}
	return NULL;
}

void ScriptSave_Core(VIRTUAL_FILE *f, const INFOS_OBJET *o, int type)		{
	int i;
	for (i=0;i<o->taille;i++)		{
		//Sauvegarde
		char temp[512], *val;
		if (o->objet[i].flags & FLAG_NOSAVE)
			continue;
		if (o->objet[i].flags & FLAG_NOSAVEGAME && type & SAVETYPE_GAME)
			continue;

		//SPECIAL: for games, only save the things which have changed
		if (type & SAVETYPE_CHANGESONLY)		{
			int *value = (int*)o->objet[i].handle;

			//For tables, we have a lot of work because we must check every entry!
			if (o->objet[i].type == TYPE_TABLEAU)			{
				INFOS_TABLEAU *itCurrent = (INFOS_TABLEAU*)value, *itDefault;
				value = (int*)itCurrent->tableau;
				int *adr = getOffsetedAddr(value);
				if (adr)		{
					int i;

					for (i=0;i<itCurrent->taille;i++)			{
						if (itCurrent->type==TYPE_ENTIER)			{
							if (((int*)itCurrent->tableau)[i] != adr[i])
								break;
						}
					}

					//Aucune entrée différente trouvée
					if (i >= itCurrent->taille)
						continue;
				}
			}
			//For values (only integers and char values supported yet) it's pretty much the same
			else			{
				int *adr = getOffsetedAddr(value);

				if (adr)		{
					switch (o->objet[i].type & MASQUE_TYPE)			{
						case TYPE_ENTIER:
						case TYPE_BOOL:
							//If they are the same, we skip this entry
							if (*value == *adr)
								continue;
							break;

						case TYPE_CHAINE:
							//If they are the same, we skip this entry
							if (!strcmp((char*)value, (char*)adr))
								continue;
							break;
					}
				}
			}
		}

		safe_strcpy(temp, gblVarName, sizeof(temp));
		//Objet.valeur
		if (gblVarName[0])
			safe_strcat(gblVarName, ".", sizeof(gblVarName));
		safe_strcat(gblVarName, o->objet[i].nom, sizeof(gblVarName));
		//Objet => profondeur
		if (o->objet[i].type == TYPE_OBJET)			{
			const char *commentaires[][2]=		{
				{"file", "\r\n#File options\r\n"},
				{"video", "\r\n#Video options\r\n"},
				{"sound", "\r\n#Sound options\r\n"},
				{"ctrl", "\r\n#Input options\r\n"},
				{"misc", "\r\n#Misc options\r\n"},
				{"music", "\r\n#About the music player\r\n"},
				{"gameboy", "\r\n#Specific to the Game Boy\r\n"},
			};
			int j;
			for (j=0;j<numberof(commentaires);j++)		{
				if (!strlwrmixcmp(commentaires[j][0], gblVarName))
					VirtualFilePuts(commentaires[j][1], f);
			}
			ScriptSave_Core(f, (INFOS_OBJET*)o->objet[i].handle, type);
		}
		else 		{
			val = GetVariableValue(&o->objet[i], 1);
			if (val)		{
				safe_strcat(gblVarName, " = ", sizeof(gblVarName));
				VirtualFilePuts(gblVarName, f);
				safe_strcat(val, "\r\n", TAILLE_MAX_CHAINES);
				VirtualFilePuts(val, f);
			}
		}
		//Restauration (objet parent)
		safe_strcpy(gblVarName, temp, sizeof(gblVarName));
	}
}

void ScriptSave(const char *filename, int type)		{
	VIRTUAL_FILE *f;
	char path[MAX_PATH];

	//Create the path if it doesn't exist
	extractFilePath((char*)filename, path, 1);
	makeDir(path);
	f = VirtualFileOpen((void*)filename, 0, VF_FILE, VF_O_WRITE);
	if (f)		{
		VirtualFilePuts("# *** Config file for MasterBoy - version 1.0 ***\r\n", f);
		gblVarName[0] = 0;
		ScriptSave_Core(f, infoObjMain, type);
		VirtualFileClose(f);
	}
}

static void ScriptBegin()		{
}

static void ScriptEnd()		{
	//Mise à jour des menus
	UpdateMenus(MENUUPD_ALL);
}

#define TAILLE_TEXTE_PROCEDURE 48
void ScriptAjouteProcedures_Add(SUBMENU *s, char *procname)		{
	//Max 64 éléments
//	if (s->nbItems >= 64)
//		return;
	s->items = realloc(s->items, sizeof(SUBMENUITEM) * (s->nbItems + 1));
	s->data = realloc(s->data, TAILLE_TEXTE_PROCEDURE * (s->nbItems + 1));
	memset(&s->items[s->nbItems], 0, sizeof(SUBMENUITEM));
	memset(s->data + TAILLE_TEXTE_PROCEDURE * s->nbItems, 0, TAILLE_TEXTE_PROCEDURE);
	s->items[s->nbItems].name = (char*)s->data + TAILLE_TEXTE_PROCEDURE * s->nbItems;
	s->items[s->nbItems].prop1 = s->nbItems;
	safe_strcpy(s->items[s->nbItems].name, procname, TAILLE_TEXTE_PROCEDURE);
//	s->items[s->nbItems].prop1 = s->nbItems;
	s->nbItems++;
}

void ScriptAjouteProcedures(char *nom_fichier, SUBMENU *s, char *startText)		{
	VIRTUAL_FILE *f;
	char str[1000];
	int i,j;

	if (s->data)
		free(s->data);
	if (s->items)
		free(s->items);
	s->items = NULL;
	s->data = NULL;
	s->nbItems = 0;

	if (startText)
		ScriptAjouteProcedures_Add(s, startText);

	f = VirtualFileOpen((void*)nom_fichier, 0, VF_FILE, VF_O_READ);
	if (!f)
		return;

	while(!VirtualFileEof(f))		{
		VirtualFileGets(str,sizeof(str),f);
		if (!str[0])
			continue;

		//Tout d'abord: ne pas être "case sensitive"...
		for (i=0;str[i];i++)	{
//			if (str[i]>='A' && str[i]<='Z')
//				str[i]+=32;

			if (str[i]=='#' || str[i]==';'/* || str[i]=='\'' || str[i]=='/'*/)		{
				str[i]=0;
				if (i>0)	{
					i--;
					while(str[i]==' ' || str[i]=='	' && i>=0)
						str[i--]=0;
				}
				goto parsingTermine;
			}
		}

parsingTermine:

		//Sauter les espaces/tabs éventuels
		i=j=0;
		while(str[i]==' ' || str[i]=='	')
			i++;

		//Bon, c'est pas le tout, mais on a un label à trouver nous!
		j=i;
		while(!(str[i]==':' && str[i+1]==0) && str[i])
			i++;
		if (!str[i])									//Pas un label
			continue;
		str[i]=0;
		if (str[j]!='_')								//Pas un label exporté (_label:)
			continue;
		ScriptAjouteProcedures_Add(s, str + j + 1);
	}

	//Met à jour les pointeurs (qui bougent avec realloc)
	for (i=0;i<s->nbItems;i++)
		s->items[i].name = (char*)s->data + TAILLE_TEXTE_PROCEDURE * i;
//	notification=1;
	VirtualFileClose(f);
}

static int atox(char *chn)	{
	int i;
	int n;
	int v;
	char *deb;
	deb=chn;
	while((*chn>='0' && *chn<='9') || (*chn>='a' && *chn<='f') || (*chn>='A' && *chn<='F'))
		chn++;
	chn--;
	i=1;
	n=0;
	while(chn>=deb)	{
		v=*(chn)-48;
		if (v>=17+32)	v-=39;
		if (v>=17)		v-=7;
		n+=i*v;
		i*=16;
		chn--;
	}
	return n;
}

//Idem mais en binaire
static int atob(char *chn)	{
	int i;
	int n;
	int v;
	char *deb;
	deb=chn;
	while(*chn=='0' || *chn=='1')
		chn++;
	chn--;
	i=1;
	n=0;
	while(chn>=deb)	{
		v=*(chn)-48;
		n+=i*v;
		i*=2;
		chn--;
	}
	return n;
}

char *ProchArgEntier(char *ptr)		{
	int compteParentheses=0;

	while(1)		{
		if (*ptr==0)
			break;
		if (*ptr=='(')
			compteParentheses++;
		if (compteParentheses==0)		{
			if (*ptr==',' || *ptr==' ' || *ptr==')')		{
				ptr++;
				break;
			}
		}
		if (*ptr==')')
			compteParentheses--;
		ptr++;
	}
	while(*ptr==' ' || *ptr=='	')
		ptr++;
	return ptr;
}

int GetEntier(char *valeur)		{
	int r,v,b;

	if ((*valeur<'0' || *valeur>'9') && *valeur != '-')		{
		if (*valeur=='t' || *valeur=='v')
			return 1;
		else if (*valeur=='f')
			return 0;
		else if (!strncmp(valeur,"rgb",3))		{
			valeur+=3;
			while(*valeur!='(' && *valeur)	valeur++;
			if (!(*valeur))
				return 0;
			valeur++;
			r=GetEntier(valeur);
			valeur=ProchArgEntier(valeur);
			v=atoi(valeur);
			valeur=ProchArgEntier(valeur);
			b=GetEntier(valeur);
			return RGB(r,v,b);
		}
		return 0;
	}
	if (*valeur=='0')		{
		valeur++;
		if (*valeur=='x')
			return atox(valeur+1);
		else if (*valeur=='b')		{
			return atob(valeur+1);
		}
		else
			return atoi(valeur-1);
	}
	return atoi(valeur);
}

char *GetChaine(char **dest)		{
	char *retour, *chne;
	chne=*dest;
	if (*chne=='\"')		{
		chne++;
		retour=chne;
		while(*chne!='\"' && *chne)
			chne++;
	}
	else if (*chne=='\'')		{
		chne++;
		retour=chne;
		while(*chne!='\'' && *chne)
			chne++;
	}
	else if (*chne=='*')		{
		chne++;
		retour=chne;
		while(*chne!='*' && *chne)
			chne++;
	}
	else
		//Chaîne telle quelle
		return chne;
	if (!(*chne))
		return NULL;
	*chne=0;
	*dest=chne+1;
	return retour;
}

const IDENTIFICATEUR *TrouveIdentificateur(const IDENTIFICATEUR *source, int taille, char *objet)
{
	int i, bNotFound = 0;
	if (!source)		{
		source = (const IDENTIFICATEUR*)objets;
		taille = sizeof(objets)/sizeof(objets[0]);
		bNotFound = 1;
	}
	//Trouve la référence d'objet dans la table
	for (i=0;i<taille;i++)		{
		//Ne prend pas la casse de 'objet'
		if (!strlwrmixcmp(objet,(char*)source[i].nom))
			return &source[i];
	}
	return NULL;
}

void SetVariableValue(const IDENTIFICATEUR *id, char *ptr)		{
	char* (*laFonction)(int, char*);
	char *arg;
	int i, val;
	switch (id->type & MASQUE_TYPE)			{
		case TYPE_FONCTION:
			//Depuis que je m'amuse sur GBA avec ce genre de trucs, ça me semble déjà plus sûr comme méthode. Ne vous inquiétez pas si vous trouvez ça sale :p
			laFonction=(char* (*)(int, char*))id->handle;
			laFonction(FCT_SET, ptr);
			break;

		case TYPE_ENTIER:
		case TYPE_BOOL:
			val=GetEntier(ptr);
			*(int*)id->handle=val;
			break;
		case TYPE_REEL:
			*(double*)id->handle=atof(ptr);
			break;

		case TYPE_CHAINE:
			arg=GetChaine(&ptr);
			if (arg)			{
				int maxSize = 1 << TYPE_MAXSIZE_MASK(id->type);
				ptr=(char*)id->handle;
				for (i=0;i<maxSize-1 && *arg;i++)
					*ptr++=*arg++;
				(*ptr)=0;
			}
			break;
	}
}

//WithQuotes: pour inclure les guillemets autour des chaînes et autres
char *GetVariableValue(const IDENTIFICATEUR *id, int withQuotes)		{
	char* (*laFonction)(int, char*);
	int i;

	switch (id->type & MASQUE_TYPE)			{
		case TYPE_FONCTION:
			//Depuis que je m'amuse sur GBA avec ce genre de trucs, ça me semble déjà plus sûr comme méthode. Ne vous inquiétez pas si vous trouvez ça sale :p
			laFonction=(char* (*)(int, char*))id->handle;
			return laFonction(FCT_GET, NULL);

		case TYPE_ENTIER:
			sprintf(tempValue, "%i", *(int*)id->handle);
			return tempValue;

		case TYPE_BOOL:
			strcpy(tempValue, (*(int*)id->handle) ? "true" : "false");
			return tempValue;

		case TYPE_TABLEAU:
			;
			INFOS_TABLEAU *it = (INFOS_TABLEAU*)id->handle;
			char tempValueTab[TAILLE_MAX_CHAINES];
			safe_strcpy(tempValueTab, "{", TAILLE_MAX_CHAINES);
			for (i=0;i<it->taille;i++)			{
				IDENTIFICATEUR ident;
				char *val;
				int maxSize;
				switch (it->type & MASQUE_TYPE)			{
					case TYPE_ENTIER:
					case TYPE_BOOL:
						ident.handle = (int*)it->tableau+i;
						break;
					case TYPE_CHAINE:
						maxSize = 1 << TYPE_MAXSIZE_MASK(it->type);
						ident.handle = (char (*)[maxSize])it->tableau+i;
						break;
					case TYPE_REEL:
						ident.handle = (double*)it->tableau+i;
						break;
				}
				ident.nom = NULL;
				ident.type = it->type;
				val = GetVariableValue(&ident, 1);
				if (val)		{
					safe_strcat(tempValueTab, val, TAILLE_MAX_CHAINES);
					//Sépare les éléments d'une virgule, sauf le dernier
					if (i < it->taille - 1)
						safe_strcat(tempValueTab, ", ", TAILLE_MAX_CHAINES);
				}
			}
			safe_strcpy(tempValue, tempValueTab, TAILLE_MAX_CHAINES);
			safe_strcat(tempValue, "}", TAILLE_MAX_CHAINES);
			return tempValue;

		case TYPE_REEL:
			sprintf(tempValue, "%f", *(float*)id->handle);
			return tempValue;

		case TYPE_CHAINE:
			if (!withQuotes)
				return (char*)id->handle;
			else		{
				safe_strcpy(tempValue, "\"", TAILLE_MAX_CHAINES);
				safe_strcat(tempValue, (char*)id->handle, TAILLE_MAX_CHAINES);
				safe_strcat(tempValue, "\"", TAILLE_MAX_CHAINES);
				return tempValue;
			}

		default:
			return NULL;
	}
}

int OuvreFichierConfig(char *nom_fichier, char *fonction)		{
	VIRTUAL_FILE *f;

	retourScript=1;

	if (!(*nom_fichier))
		return 0;

	f = VirtualFileOpen((void*)nom_fichier, 0, VF_FILE, VF_O_READ);
	if (!f)
		return 0;

	return ExecScript(f,fonction, NULL);
}

//Command: si FILE == NULL
int ExecScript(VIRTUAL_FILE *f, char *fonction, char *command)		{
	char str[1000], objet[1000], commande[1000], *ptr, tmp[4096], fonction_reel[1000];
	int i,j, type;
	void *handle;
	INFOS_OBJET *infos;
	int indice;
	const IDENTIFICATEUR *idObjet, *idEnCours;
	int tailleEnCours;

	arretdurgence=0;

	retourScript=1;

	if (fonction)		{				//Une fonction sur laquelle se brancher?
		safe_strcpy(fonction_reel, fonction, sizeof(fonction_reel));
		fonction = fonction_reel;
		//Pas case sensitive, bien entendu :p
		for (ptr=fonction;*ptr;ptr++)
			if (*ptr>='A' && *ptr<='Z')
				*ptr+=32;
	}

	trouveLabel=fonction;

	ScriptBegin();

	while(f?(!VirtualFileEof(f)):(*command))		{
		if (arretdurgence)
			break;
		if (f)
			VirtualFileGets(str,sizeof(str),f);
		else	{
			i=0;
			while(*command != ';' && *command && i<sizeof(str)-1)
				str[i++] = *command++;
			if (*command == ';')
				command++;
//			str[i++]='\n';
			str[i]=0;
		}

		if (!str[0])
			continue;

		//Tout d'abord: ne pas être "case sensitive"...
		for (i=0;str[i];i++)	{
			if (str[i]>='A' && str[i]<='Z')
				str[i]+=32;

			if (str[i]=='\"')	{
				i++;
				while(str[i] && (str[i]!='\"' || (str[i-1]=='\\' && str[i]=='\"')))
					i++;
				if (!str[i])
					break;
			}
			else if (str[i]=='\'')	{
				i++;
				while(str[i] && (str[i]!='\'' || (str[i-1]=='\\' && str[i]=='\'')))
					i++;
				if (!str[i])
					break;
			}
			else if (str[i]=='*')	{
				i++;
				while(str[i] && (str[i]!='*' || (str[i-1]=='\\' && str[i]=='*')))
					i++;
				if (!str[i])
					break;
			}
			else if (str[i]=='#' || str[i]==';')		{
				str[i]=0;
				if (i>0)	{
					i--;
					while(str[i]==' ' || str[i]=='	' && i>=0)
						str[i--]=0;
				}
				goto parsingTermine;
			}
		}
		i--;

parsingTermine:
		if (str[i]==':' && !trouveLabel)		//Label trouvé!
			continue;

		idEnCours = NULL;
		tailleEnCours = 0;
		i = 0;
		indice = -1;

continueParse:
		j = 0;
		//Sauter les espaces/tabs éventuels
		while(str[i]==' ' || str[i]=='	')
			i++;

		//Bon, c'est pas le tout, mais on a un label à trouver nous!
		if (trouveLabel)		{
			if (str[i]=='_')
				i++;
			j=i;
			while(!(str[i]==':' && str[i+1]==0) && str[i])
				i++;
			if (!str[i])									//Pas un label
				continue;
			str[i]=0;
			if (strlwrmixcmp(str+j, trouveLabel))			//Pas le bon label
				continue;
			trouveLabel=NULL;								//Label trouvé -> plus à rechercher.
			continue;
		}

		//Après, récupérer l'objet de la commande (opérande gauche)
		while(str[i] && str[i]!=' ' && str[i]!='	' && str[i]!='.' && str[i]!='=' && str[i]!='[')
			objet[j++]=str[i++];

		//Si ça n'a pas été terminé par un point
		while(str[i]!='[' && str[i]!='.' && str[i]!='=' && str[i] && str[i]!=' ')		i++;

		//Termine la chaîne
		objet[j]=0;

		//L'objet est faux?
		if (!objet[0])
			continue;

		//Permet ensuite de savoir si aucune commande n'a été donnée
		commande[0]=0;

		if (str[i]=='=' || str[i]==' ' || str[i]=='.' || str[i]=='[')				//Pas de méthode
			goto sauteEspaces;

		//Fin de chaîne -> seul un objet est spécifié
		if (!str[i])
			goto sauteEspaces;

		//Saute les espaces
		while(str[i]==' ' || str[i]=='	')	i++;

//commande:
		//Après, récupérer la commande à exécuter (opérande droite)
		j=0;
		while(str[i] && str[i]!=' ' && str[i]!='	' && str[i]!='=' && str[i]!='[')
			commande[j++]=str[i++];

		//Termine la chaîne
		commande[j]=0;

		//Sauter les espaces/tabulations qui viennent ensuite
sauteEspaces:
		if (str[i]==' ' || str[i]=='	')		{
			while(str[i]==' ' || str[i]=='	')
				i++;
		}

		if (str[i]=='.')	{
			i++;
//			goto commande;
		}
		if (str[i]=='=')	{
			i++;
			goto sauteEspaces;
		}
		if (str[i]=='[')	{
			i++;
			indice=GetEntier(str+i);
			while(str[i] && str[i]!=']')
				i++;
			if (str[i])		{
				i++;
				goto sauteEspaces;
			}
		}

		//On est prêt pour exécuter une commande!
		ptr=str+i;

		handle=NULL;
		gblFichierConfig = f;

		idObjet = TrouveIdentificateur(idEnCours, tailleEnCours, objet);

		//Aucun résultat
		if (!idObjet)		{
			if (idEnCours)
				sprintf(tmp, "The property %s doesn't exist.", objet);
			else
				sprintf(tmp, "The identifier %s doesn't exist.", objet);
			ScriptError(tmp);
			arretdurgence = 1;
			continue;
		}
		else		{
			handle = idObjet->handle;
			type = idObjet->type;
		}

		gblIndice=indice;

		if (type==TYPE_OBJET)		{
			infos = (INFOS_OBJET*)handle;

			idEnCours = infos->objet;
			tailleEnCours = infos->taille;
			goto continueParse;
		}

		if (type==TYPE_TABLEAU)		{
			INFOS_TABLEAU *it;
			it = (INFOS_TABLEAU*)handle;
			if (indice < 0)		{
				if (*ptr == '{')			{
					//Callback avant
//					if (idObjet->flags & FLAG_CALLBACK)
//						return ((char* (*)(int, char*))idObjet->handle2)(FCT_SET_BEFORE, ptr);
					//Saute le {
					ptr++;
					SAUTE_ESPACES(ptr);
					//CHAÎNES PAS TESTÉES!!!!
					for (i=0;i<it->taille;i++)		{
						char *val = NULL, value[TAILLE_MAX_CHAINES];
						handle = NULL;
						//Plus rien
						if (!(*ptr))
							break;

						switch (it->type & MASQUE_TYPE)			{
							case TYPE_ENTIER:
								handle=(int*)it->tableau+i;
								val = value;
								sprintf(value, "%i", GetEntier(ptr));
								ptr = ProchArgEntier(ptr);
								break;

							case TYPE_BOOL:
								handle=(int*)it->tableau+i;
								val = value;
								strcpy(value, GetEntier(ptr) ? "true" : "false");
								ptr = ProchArgEntier(ptr);
								break;

							case TYPE_CHAINE:
								;
								int maxSize = 1 << TYPE_MAXSIZE_MASK(it->type);
								handle=(char (*)[maxSize])it->tableau+i;
								val = GetChaine(&ptr);
								break;
						}

						if (handle && val)		{
							IDENTIFICATEUR ident;
							ident.nom = NULL;
							ident.type = it->type;
							ident.handle = handle;
							SetVariableValue(&ident, val);
						}
					}
					//Callback après
					if (idObjet->flags & FLAG_CALLBACK)
						((char* (*)(int, char*))idObjet->handle2)(FCT_SET_AFTER, NULL);
				}
				else	{
					//Le tableau ne peut être évalué (erreur de syntaxe)
					if (idObjet->flags & FLAG_CALLBACK)
						((char* (*)(int, char*))idObjet->handle2)(FCT_SET, ptr);
					else
						goto error1;
				}
			}
			else	{
				IDENTIFICATEUR ident;
				if (indice < 0 || indice >= it->taille)		{
error1:
					if (indice == -1)
						sprintf(tmp,"Array %s.\nSyntax error.", objet, indice);
					else
						sprintf(tmp,"Array %s.\nThe subscript (%i) is out of bounds.", objet, indice);
					ScriptError(tmp);
					continue;
				}

				switch (it->type & MASQUE_TYPE)			{
					case TYPE_ENTIER:
					case TYPE_BOOL:
						ident.handle=(int*)it->tableau+indice;
						break;

					case TYPE_CHAINE:
						;
						int maxSize = 1 << TYPE_MAXSIZE_MASK(it->type);
						ident.handle=(char (*)[maxSize])it->tableau+indice;
						break;

					case TYPE_REEL:
						ident.handle=(double*)it->tableau+indice;
						break;

					default:
						//Vous devriez pouvoir adapter pour d'autres types sans trop de problèmes ;)
						sprintf(tmp, "The array %s has an uncomputable type.", objet);
						ScriptError(tmp);
						continue;
				}
				ident.nom = NULL;
				ident.type = it->type;
				SetVariableValue(&ident, ptr);
				//Callback éventuel
				if (idObjet->flags & FLAG_CALLBACK)
					((char* (*)(int, char*))idObjet->handle2)(FCT_SET_AFTER, NULL);
			}
			continue;
		}

		SetVariableValue(idObjet, ptr);
		//Callback éventuel
		if (idObjet->flags & FLAG_CALLBACK)
			((char* (*)(int, char*))idObjet->handle2)(FCT_SET_AFTER, NULL);
//		if (arretdurgence)
//			break;
	}

	ScriptEnd();
	if (f)
		VirtualFileClose(f);
	return (trouveLabel == NULL);
}

const IDENTIFICATEUR *GetVariableAddress(char *str)		{
	char objet[1000], *ptr;
	int i,j, type;
	void *handle;
	INFOS_OBJET *infos;
	int indice;
	const IDENTIFICATEUR *idObjet, *idEnCours;
	int tailleEnCours;

	idEnCours = NULL;
	tailleEnCours = 0;
	i = indice = 0;

continueParse:
	j = 0;
	//Sauter les espaces/tabs éventuels
	while(str[i]==' ' || str[i]=='	')
		i++;

	//Après, récupérer l'objet de la commande (opérande gauche)
	while(str[i] && str[i]!=' ' && str[i]!='	' && str[i]!='.' && str[i]!='[')
		objet[j++]=str[i++];

	//Si ça n'a pas été terminé par un point
	while(str[i]!='[' && str[i]!='.' && str[i] && str[i]!=' ')		i++;

	//Termine la chaîne
	objet[j]=0;

	//L'objet est faux?
	if (!objet[0])
		return NULL;

	if (str[i]==' ' || str[i]=='.' || str[i]=='[')				//Pas de méthode
		goto sauteEspaces;

	//Fin de chaîne -> seul un objet est spécifié
	if (!str[i])
		goto sauteEspaces;

	//Saute les espaces
	while(str[i]==' ' || str[i]=='	')	i++;

	//Sauter les espaces/tabulations qui viennent ensuite
sauteEspaces:
	if (str[i]==' ' || str[i]=='	')		{
		while(str[i]==' ' || str[i]=='	')
			i++;
	}

	if (str[i]=='.')	{
		i++;
//			goto commande;
	}
	if (str[i]=='[')	{
		i++;
		indice=GetEntier(str+i)-1;
		while(str[i] && str[i]!=']')
			i++;
		if (str[i])		{
			i++;
			goto sauteEspaces;
		}
	}

	//On est prêt pour exécuter une commande!
	ptr=str+i;

	handle=NULL;

	idObjet = TrouveIdentificateur(idEnCours, tailleEnCours, objet);

	//Aucun résultat
	if (!idObjet)		{
		return NULL;
	}
	else		{
		handle = idObjet->handle;
		type = idObjet->type;
	}

	gblIndice=indice;

	if (type==TYPE_OBJET)		{
		infos = (INFOS_OBJET*)handle;

		idEnCours = infos->objet;
		tailleEnCours = infos->taille;
		goto continueParse;
	}
	return idObjet;
}

static void ScriptError(char *msg)		{
	MessageBox(msg, "Script error", MB_OK);
	arretdurgence = 1;
}
