#include "sound_int.h"
#include "sound.h"
#include "gym/gym.h"
#include "zlib.h"
#include "unzip.h"
#include <pspaudio.h>
#include "pspcommon.h"
#include "psp/PspSound.h"
#include <psphprm.h>

CODEC_GYM::input_gym *gymCodec = NULL;

#define BUF_SIZE 2048

#define MAX_BUFFER_LENGTH 16384
#define MAX_BUFFER_LENGTH_MASK 16383
const int buffPerFrame = BUF_SIZE;
char menuSoundTitle[128], menuSoundAlbum[128], menuSoundArtist[128];
int menuSoundBoxActive = 0;

char menu_zipfilename[MAX_PATH];
int menu_musiclen = 0, menu_lastNumber;
void *menu_musicdata = NULL;
int musicEnabled = 0;
volatile int musicThread = -1;
int musicThreadFrequency;
//Si vaut 1, quitter
int musicThreadEnd = 0, musicRequestNewSong = 0;
u32 snd_buffer[MAX_BUFFER_LENGTH], tmp_buffer[2][buffPerFrame];
s64 buffer1 = 0, buffer2 = 0;
u32 bufferValue;
u32 lastSample;
//Numéro de fichier / nombre d'éléments dans la playlist
int menu_filenumber, menu_playlistcount;
//-1 = disabled, 0 = stopped, 1 = playing, 2 = paused, 3 = sleep
int menuMusicState = -1;
int musicVolume = 0x8000, musicChannelVolume = 0;
int isVgmInited = 0, songType = 0;
enum {SONG_VGM, SONG_GYM};
CFILE *currentSongFile = NULL;

void VGM_Init();
int VGM_play(char *fn, CFILE *r, SONGINFO * info, unsigned int flags);
int VGM_DecodeThread(short *SampleBuffer, int length, int *finallen);
void VGM_SetFrequency(int frequency);
u32 ramAvailable (void);

//int musicThreadFinalValue = -1;

void fctMsgBoxSpeaker_Close(WINDOW *w, int valid)			{
	if (valid)		{
		if (menuIsInGame == 2)
			SoundResume();
		else		{
			if (musicThread == 0)
				musicThread = -1;
			menuMusicPlay();
		}
	}
	else	{
		if (menuIsInGame == 2)		{
			menuConfig.sound.enabled = 0;
			SoundTerm();
		}
		else
			menuMusicStop();
	}
}

void fctMsgBoxSpeaker_Handle(WINDOW *w, int valid)			{
}


void MenuSpecialMessageDisplay()			{
	fctMsgBoxSpeaker_Close(NULL, MessageBoxEx("Your headphones are plugged out. Do you want to use the speaker?\nIf you choose no, sound will be disabled (you can re-enable it in the menu: Sound, Enabled, On)", "Question", MB_YESNO, 250));
	menuDisplaySpecialMessage = 0;
}

void StandardSoundProcess()		{
	static int hpConnected = 0;
	int curState = sceHprmIsHeadphoneExist();
	if (!curState && hpConnected)		{
		if (menuIsInGame == 2)		{
			SoundPause();
			menuDisplaySpecialMessage = 1;
		}
		else	{
			MessageBoxAsync("Your headphones are plugged out. Do you want to continue playback with the speaker?", "Question", MB_YESNO, 200, fctMsgBoxSpeaker_Close, fctMsgBoxSpeaker_Handle);
//			musicThreadFinalValue = -1;
			menuMusicPause();
			menuMusicState = 3;
		}
	}
	hpConnected = curState;
}

//Interpolé
/*void FillBufferArbitrary(int frame)		{
	int i;
	u32 val = bufferValue;
	u32 sample = lastSample;
	u32 valIncr = (musicThreadFrequency * 256) / 44100;
	u16 *ptr = (u16*)tmp_buffer[frame];

	for (i=0;i<buffPerFrame;i++)		{
		val += valIncr;
		if (val >= 256)		{
			lastSample = sample;
			sample = snd_buffer[buffer1 & MAX_BUFFER_LENGTH_MASK];
			buffer1++;
			val -= 256;
		}
//		tmp_buffer[frame][i] = lastSample;
		*ptr++ = ((lastSample & 0xffff) * (256 - val) + (sample & 0xffff) * val) >> 8;
		*ptr++ = ((lastSample >> 16) * (256 - val) + (sample >> 16) * val) >> 8;
	}
	bufferValue = val;
	lastSample = sample;
}*/

//Pas interpolé
void FillBufferArbitrary(int frame)		{
	int i;
	int val = bufferValue;
	u32 sample = lastSample;
	for (i=0;i<buffPerFrame;i++)		{
		val += musicThreadFrequency;
		if (val >= 44100)		{
			sample = snd_buffer[buffer1 & MAX_BUFFER_LENGTH_MASK];
			buffer1++;
			val -= 44100;
		}
		tmp_buffer[frame][i] = sample;
	}
	bufferValue = val;
	lastSample = sample;
}

void FillBuffer44100(int frame)		{
	int i;
	for (i=0;i<buffPerFrame;i++)		{
		tmp_buffer[frame][i] = snd_buffer[buffer1 & MAX_BUFFER_LENGTH_MASK];
		buffer1++;
	}
}

int gymThread(SceSize u1, void *u2) {
	int channel = sceAudioChReserve(-1, BUF_SIZE, 0);
	int size, bps, nch, srate;
	int sourcePerFrame = -1;
	int frame = 1, success;
	bufferValue = 0;
	for (;!musicThreadEnd;) {
		u32 *buffer, bufferReal[BUF_SIZE];
		int i /*, no*/, success;
//		no = 0;
		success = 1;
		while (buffer1 + buffPerFrame > buffer2 /*&& no < 3*/)		{
			if (songType == SONG_GYM)		{
				if (!gymCodec->get_samples_pcm((void**)&buffer, &sourcePerFrame, &srate, &bps, &nch))			{
					musicRequestNewSong = 1;
					goto end;
				}
				sourcePerFrame >>= 2;
			}
			else if (songType == SONG_VGM)		{
				buffer = bufferReal;
				if (VGM_DecodeThread((short*)buffer, BUF_SIZE >> 2, &sourcePerFrame))
					success = 0;
//				sourcePerFrame = BUF_SIZE;
			}
			for (i=0;i<sourcePerFrame;i++)		{
				snd_buffer[buffer2 & MAX_BUFFER_LENGTH_MASK] = buffer[i];
				buffer2++;
			}
//			no++;
		}
		//Trop de retard
		if (buffer1 + buffPerFrame > buffer2)
			buffer1 = buffer2 - buffPerFrame;
		frame = 1 - frame;
		if (musicThreadFrequency == 44100)
			FillBuffer44100(frame);
		else
			FillBufferArbitrary(frame);
		if (musicChannelVolume < musicVolume)
			musicChannelVolume = musicChannelVolume * 3 / 2 + 2;
		if (musicChannelVolume > musicVolume)
			musicChannelVolume = musicVolume;
		sceAudioOutputPannedBlocking(channel, musicChannelVolume, musicChannelVolume, (char*)tmp_buffer[frame]);
		//C'était la dernière sample
		if (!success)			{
			musicRequestNewSong = 1;
			goto end;
		}
		StandardSoundProcess();
	}
end:
	sceAudioChRelease(channel);
/*	musicThread = musicThreadFinalValue;
	musicThreadFinalValue = 0;*/
	musicThread = -1;
	sceKernelExitDeleteThread(0);
}

/*int menuPlaySound()		{
	CFILE file;
	SONGINFO song;
	if (musicThread >= 0 && !musicThreadEnd)		{
		musicThreadEnd = 1;
		sceKernelWaitThreadEnd(musicThread, NULL);
		if (gymCodec)		{
			delete gymCodec;
			gymCodec = NULL;
		}
	}
	musicThread = sceKernelCreateThread("Music thread", (SceKernelThreadEntry)gymThread, 0x11, 256*1024, 0, 0 );
	if (musicThread >= 0)			{
		if (file.open("test.gym", "rb"))			{
			song.set_filename("test.gym");
			if (!gymCodec)
				gymCodec = new CODEC_GYM::input_gym;
			if (gymCodec->open(&file, &song, 0))		{
				musicThreadEnd = 0;
				sceKernelStartThread(musicThread, 0, 0 );
				return 1;
			}
		}
	}
	return 0;
}*/

static int start_music(int song)		{
	musicThreadEnd = 0;
	musicThread = sceKernelCreateThread("Music thread", (SceKernelThreadEntry)gymThread, 0x11, 256*1024, 0, 0 );
	songType = song;
	if (musicThread >= 0)
		sceKernelStartThread(musicThread, 0, 0 );
	else
		return 0;
	return 1;
}

int menuPlaySoundEx(CFILE *f)		{
	SONGINFO song;
	int success = 0;

	if (musicThread >= 0 /*&& !musicThreadEnd*/)		{
		musicThreadEnd = 1;
		sceKernelWaitThreadEnd(musicThread, NULL);
		//Nécessaire pour éviter qu'il ne soit pas effacé dans le cas où un morceau ne peut être chargé
		sceKernelDeleteThread(musicThread);
	}
/*	if (gymCodec)		{
		delete gymCodec;
		gymCodec = NULL;
	}*/
	if (f)			{
		int filetype;
		char *extension = strstr(f->name, ".");
		if (!extension)
			extension = "";
		
		//Tags par défaut
		strcpy(menuSoundTitle, "<Title unknown>");
		strcpy(menuSoundAlbum, "<Artist unknown>");

		//Type GYM (Genesis YM2612)
		if (!strcmp(extension, ".gym"))			{
			song.set_filename("test.gym");
			if (!gymCodec)
				gymCodec = new CODEC_GYM::input_gym;
			else
				gymCodec->reset();
			musicThreadFrequency = menuConfig.music.frequency;
			gymCodec->set_frequency(menuConfig.music.frequency);
			if (gymCodec->open(f, &song, 0))		{
				start_music(SONG_GYM);
				success = 1;
			}
		}
		//Type VGM (Video Game Music) ou VGZ (VGM g-zippé)
		else if (!strcmp(extension, ".vgm"))			{
			song.set_filename("test.vgm");
			if (!isVgmInited)
				VGM_Init();
			musicThreadFrequency = menuConfig.music.frequency;
			VGM_SetFrequency(menuConfig.music.frequency);
			if (!VGM_play(NULL, f, &song, 0))		{
//					u32 buffer[BUF_SIZE];
//					VGM_DecodeThread((short*)buffer, BUF_SIZE >> 2);
				start_music(SONG_VGM);
				success = 1;
			}
		}
	}

	if (success)		{
		musicEnabled = 1;
		menuMusicState = 1;
		menuSoundBoxActive = MENU_SOUND_BOX_ACTIVE_MAX;
	}
	return success;
}

static void _cleanup()		{
	if (gymCodec)		{
		delete gymCodec;
		gymCodec = NULL;
	}
	if (currentSongFile)		{
		delete currentSongFile;
		currentSongFile = NULL;
	}
}

static void cleanup()		{
	menuMusicStop();
	_cleanup();
}

int menuPlayZipSound(char *filename, int number)				{
	unzFile zip_file;
	unz_file_info unzinfo;
	char nom[256];
	int l;
//	int tryagain = 0;

	zip_file = 0;    
	zip_file = unzOpen(filename);
	if (zip_file) {
		unz_global_info pglobal_info;

		unzGetGlobalInfo(zip_file,&pglobal_info);
		menu_playlistcount = pglobal_info.number_entry;

		if (number == -1)			{
			struct timeval now;
			sceKernelLibcGettimeofday( &now, 0 );
			srand((now.tv_usec+now.tv_sec*1000000));
			do		{
				number = rand()%pglobal_info.number_entry;
			} while (menu_filenumber == number && pglobal_info.number_entry > 1);
			menu_filenumber = number;
		}

		unzGoToFirstFile(zip_file);
		while (number--) {
			unzGoToNextFile(zip_file);
		}
		if (unzGetCurrentFileInfo(zip_file, &unzinfo, nom, sizeof(nom), NULL, 0, NULL, 0) != UNZ_OK) {
			unzClose (zip_file);
			return 0;
		}

		//Libérer les données qui existaient déjà - réalisé par le blockfree du currentSongFile
/*		if (menu_musicdata)		{
			free(menu_musicdata);
			menu_musiclen = 0;
		}*/

		//If we are low on memory, first unload the current song to free up some memory
/*		int availRam = ramAvailable();
		if (availRam - (1 << 20) < menu_musiclen)
			cleanup();*/

		unzOpenCurrentFile (zip_file);
		menu_musiclen = unzinfo.uncompressed_size;
		menu_musicdata = (void*)malloc(menu_musiclen);

		//Not enough memory? Close the current song and try again...
		if (!menu_musicdata)		{
			cleanup();
			menu_musicdata = (void*)malloc(menu_musiclen);
		}

		if (menu_musicdata)
			unzReadCurrentFile(zip_file,(void*)(menu_musicdata), menu_musiclen);
		unzCloseCurrentFile (zip_file);
		unzClose (zip_file);

		if (menu_musicdata)			{
			//On doit arrêter la musique en cours à tout prix, sinon on risque d'avoir des plantages car la mémoire est libérée
			menuMusicStop();

			if (!currentSongFile)
				currentSongFile = new CFILE;
			currentSongFile->set_name(nom);
//retry:
			if (currentSongFile->open(menu_musicdata, menu_musiclen, "rb"))		{
				currentSongFile->blockfree = 1;
				if (menuPlaySoundEx(currentSongFile))
					return 1;
			}
/*			else if (!tryagain)			{
				cleanup();
				tryagain = 1;
				goto retry;
			}*/
		}
	}
	return 0;
}

// *** FUNCTIONS ***
#define RAM_BLOCK      (1024 * 1024)

u32 ramAvailableLineareMax (void)
{
 u32 size, sizeblock;
 u8 *ram;


 // Init variables
 size = 0;
 sizeblock = RAM_BLOCK;

 // Check loop
 while (sizeblock)
 {
  // Increment size
  size += sizeblock;

  // Allocate ram
  ram = (u8*)malloc(size);

  // Check allocate
  if (!(ram))
  {
   // Restore old size
   size -= sizeblock;

   // Size block / 2
   sizeblock >>= 1;
  }
  else
   free(ram);
 }

 return size;
}

u32 ramAvailable (void)
{
 u8 **ram, **temp;
 u32 size, count, x;


 // Init variables
 ram = NULL;
 size = 0;
 count = 0;

 // Check loop
 for (;;)
 {
  // Check size entries
  if (!(count % 10))
  {
   // Allocate more entries if needed
   temp = (u8**)realloc(ram,sizeof(u8 *) * (count + 10));
   if (!(temp)) break;
 
   // Update entries and size (size contains also size of entries)
   ram = temp;
   size += (sizeof(u8 *) * 10);
  }

  // Find max lineare size available
  x = ramAvailableLineareMax();
  if (!(x)) break;

  // Allocate ram
  ram[count] = (u8*)malloc(x);
  if (!(ram[count])) break;

  // Update variables
  size += x;
  count++;
 }

 // Free ram
 if (ram)
 {
  for (x=0;x<count;x++) free(ram[x]);
  free(ram);
 }

 return size;
}

void menuStartSound(char *filename)		{
	if (menuConfig.music.repeatMode < 4)
		menu_filenumber = 0;
	else
		menu_filenumber = -1;
	menuPlayZipSound(filename, menu_filenumber);
	strcpy(menu_zipfilename, filename);
	musicRequestNewSong = 0;
	const int values[] =		{
		//Normal
		0x1000,
		//+1
		0x2000,
		//+2
		0x4000,
		//+3
		0x8000
	};
	musicChannelVolume = musicVolume = values[menuConfig.sound.volume];
}

void menuMusicBrowseTrack(int trackNo, int prevNext)		{
	int alreadyDone = 0;
	//Nouvelle musique requise
	switch (menuConfig.music.repeatMode)			{
		case 0:
		case 2:
			//Jusqu'à ce qu'on trouve un bon fichier à charger...
			do			{
				menu_filenumber += prevNext;
				//Encore des fichiers?
				if (menu_filenumber >= menu_playlistcount)		{
					//Fin de la playlist?
					if (menuConfig.music.repeatMode == 0)			{
						menuMusicStop();
						musicEnabled = 0;
						menuConfig.music.filename[0] = 0;
						break;
					}
					//On recommence
					else 			{
						//Eviter les boucles infinies
						if (alreadyDone)
							break;
						menu_filenumber = 0;
						alreadyDone = 1;
					}
				}
				//Eviter de jouer un fichier inférieur à zéro
				else if (menu_filenumber < 0)		{
/*					if (menuConfig.music.repeatMode == 0)
						menu_filenumber = 0;
					else	{*/
						while (menu_filenumber < 0)
							menu_filenumber += menu_playlistcount;
//					}
				}
			} while (!menuPlayZipSound(menu_zipfilename, menu_filenumber));
			break;

		case 1:
			//On répète le même...
			menuPlayZipSound(menu_zipfilename, menu_filenumber);
			break;

		case 5:
			//Random...
			menuPlayZipSound(menu_zipfilename, -1);
			break;
	}
}


void menuMusicPrevTrack()		{
	if (menuSoundBoxActive > MENU_SOUND_BOX_PREVTRACK_TRESHOLD)
		//Quick tap => play the previous one
		menuMusicBrowseTrack(-1, -1);
	else		{
		//Replay the same track
		if (currentSongFile->open(menu_musicdata, menu_musiclen, "rb"))
			menuPlaySoundEx(currentSongFile);
	}
//		menuMusicBrowseTrack(-1, 0);
}

void menuMusicNextTrack()		{
	menuMusicBrowseTrack(-1, 1);
}



void soundStandardVblank()		{
	if (musicEnabled)		{
		const int values[] =		{
			//Normal
			0x1000,
			//+1
			0x2000,
			//+2
			0x4000,
			//+3
			0x8000
		};
		musicVolume = values[menuConfig.sound.volume];
	}
	if (musicEnabled && musicRequestNewSong)		{
		musicRequestNewSong = 0;
		menuMusicNextTrack();
	}
}

void menuMusicPlay()			{
	//Nouveau fichier?
	if (!musicEnabled && menuConfig.music.filename[0])		{
		menuStartSound(menuConfig.music.filename);
	}
	if (musicEnabled && musicThread < 0)		{
		musicThreadEnd = 0;
		musicThread = sceKernelCreateThread("Music thread", (SceKernelThreadEntry)gymThread, 0x11, 256*1024, 0, 0 );
		sceKernelStartThread(musicThread, 0, 0 );
		menuMusicState = 1;
	}
}

void menuMusicPause()			{
	if (musicEnabled && musicThread >= 0)		{
		musicThreadEnd = 1;
		sceKernelWaitThreadEnd(musicThread, NULL);
		musicThread = -1;
		menuMusicState = 2;
		musicChannelVolume = 0x200;
	}
}

void menuMusicStop()			{
	if (musicEnabled && musicThread >= 0)		{
		musicThreadEnd = 1;
		sceKernelWaitThreadEnd(musicThread, NULL);
		musicThread = -1;
		menuMusicState = 0;
	}
	_cleanup();
}
