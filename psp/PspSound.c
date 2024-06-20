
#include <pspaudio.h>
#include "pspcommon.h"
#include "shared.h"
#include "sound.h"
#include "PspSound.h"

typedef struct {
        short l, r;
} sample_t;

//#define MAXSAMPLES 4096
//4096 marche aussi mais augmente les risques de clics
#define MAXSAMPLES 8192
sample_t audioOut[2][2048], audioOutTemp[MAXSAMPLES];
int nbreSamples=0, ptrSample=0;
int sndThid = 0;
static int SoundThread();
int soundPause = 1, soundInited = 0;
s64 total1=0, total2=0;
volatile char soundRunning = -1;

void SoundWaitTerm()		{
	//Wait the old thread to be deleted
	while (soundRunning != -1 && !osl_quit)
		oslWaitVSync();
}

void SoundInit(void)
{
	if(soundInited)
		SoundTerm();

	if (!soundInited)		{
		if(snd.sample_rate)
		{
			//Wait the old thread to be deleted
			SoundWaitTerm();

			memset(audioOut, 0, 2*2048*sizeof(sample_t));

			soundRunning = 1;
			sndThid = sceKernelCreateThread("SoundThread", SoundThread, 0x11, 0xFA0, 0, 0); 

			if(sndThid<0)
				return;

			sceKernelStartThread(sndThid,0,0);  
			soundInited = 1;
		}
	}
}

void SoundTerm(void)
{
	if(soundInited)
	{
//		sceKernelTerminateDeleteThread(sndThid);

		sndThid = 0;
		soundInited = 0;
		soundRunning = 0;
//		while (soundRunning != -1 && !osl_quit);
	}
}

void SoundUpdate(void)
{
	if(menuConfig.sound.enabled)
	{
		int i, j = 0;

		for(i = 0; i < snd.sample_count; ++i) 
		{
			audioOutTemp[nbreSamples].l = snd.output[j++];
			audioOutTemp[nbreSamples].r = snd.output[j++];
			nbreSamples = (nbreSamples + 1) & (MAXSAMPLES - 1);
		}

		total1 += snd.sample_count;
		soundPause = 0;
	}
}

void SoundPause(void)
{
	soundPause = 1;
	
	//sceKernelSuspendThread(sndThid);
}

void SoundResume(void)
{
	soundPause = 0;

	//sceKernelResumeThread(sndThid);
}

void SoundResample(sample_t *audioOut, int sample_count)		{
	int i, j;
	u32 *src = (u32*)audioOutTemp, *dst = (u32*)audioOut;
	switch (snd.sample_rate)
	{
		case 22050:
			for(i = 0; i < sample_count; i++)
			{
				*dst++ = *src;
				*dst++ = *src++;
			}
			break;

		case 11025:
			for(i = 0; i < sample_count; ++i)
			{
				*dst++ = *src;
				*dst++ = *src;
				*dst++ = *src;
				*dst++ = *src++;
			}
			break;
	}

}


int AudioGet(sample_t *audioOut, int sample_count, int realSampleCount)
{
	if (menuConfig.sound.perfectSynchro)			{
		if (!soundPause)		{
			if (gblMachineType == EM_GBC)
				snd_render_orig((void*)audioOut, realSampleCount);
			else if (gblMachineType == EM_SMS)		{
				if (snd.sample_rate == 44100)
					sound_update_ex((void*)audioOut, realSampleCount);
				else		{
					sound_update_ex((void*)audioOutTemp, realSampleCount);
					SoundResample(audioOut, realSampleCount);
				}
			}
		}
		return;
	}

	int i;
	/*
		En cas normal, le chip son s'attend à avoir l'équivalent de 44100 samples par seconde.
		Dans notre cas, 44100 est divisé en 60 frames par seconde, mais en réalité la synchro LCD s'effectue seulement
		à 59.xx fps, résultat on prend périodiquement du retard. Lorsque ce retard est trop important, on saute
		simplement une sample.
	*/
	if (total2 + realSampleCount > total1)
		return;
/*	if (total2 + realSampleCount > total1)		{
		sound_update_ex(audioOut, realSampleCount);
		return;
	}*/

	/*
		Au cas où le son aurait de l'avance sur nous, on doit le rattraper.
		Problème: si on utilise le frameskip, le son peut prendre une immense avance sur nous car x frames sont générées
		rapidement avant d'être effectivement jouées, on doit donc être sûr que ce n'est pas le cas avec frameReady
	*/
	if (frameReady)			{
		while (total2 + sample_count < total1 - sample_count * 4)		{
			total2 += sample_count;
			ptrSample = (ptrSample + sample_count) & (MAXSAMPLES - 1);
		}
		frameReady = 0;
	}
	if (snd.sample_rate == 44100)			{
		if (ptrSample + sample_count < MAXSAMPLES)		{
			memcpy(audioOut, audioOutTemp + ptrSample, sample_count * sizeof(*audioOut));
			ptrSample += sample_count;
		}
		else		{
			int part1 = MAXSAMPLES - ptrSample;
			memcpy(audioOut, audioOutTemp + ptrSample, part1 * sizeof(*audioOut));
			memcpy(audioOut + part1, audioOutTemp, (sample_count - part1) * sizeof(*audioOut));
			ptrSample = (ptrSample + sample_count) & (MAXSAMPLES - 1);
		}
	}
	else if (snd.sample_rate == 22050)			{
		for(i = 0; i < sample_count; ++i)
		{
			audioOut[i].l = audioOutTemp[ptrSample].l;
			audioOut[i].r = audioOutTemp[ptrSample].r;
			++i;
			audioOut[i].l = audioOutTemp[ptrSample].l;
			audioOut[i].r = audioOutTemp[ptrSample].r;
			ptrSample = (ptrSample + 1) & (MAXSAMPLES - 1);
		}
	}
	else if (snd.sample_rate == 11025)			{
		for(i = 0; i < sample_count; ++i)
		{
			audioOut[i].l = audioOutTemp[ptrSample].l;
			audioOut[i].r = audioOutTemp[ptrSample].r;
			++i;
			audioOut[i+1].l = audioOutTemp[ptrSample].l;
			audioOut[i+1].r = audioOutTemp[ptrSample].r;
			++i;
			audioOut[i+2].l = audioOutTemp[ptrSample].l;
			audioOut[i+2].r = audioOutTemp[ptrSample].r;
			++i;
			audioOut[i+3].l = audioOutTemp[ptrSample].l;
			audioOut[i+3].r = audioOutTemp[ptrSample].r;
			ptrSample = (ptrSample + 1) & (MAXSAMPLES - 1);
		}
	}
	total2 += realSampleCount;
}


static int SoundThread()
{
	int err=0;
	int sample_count;
	int chan;
	int realSampleCount;
	int buf = 0;

	//For SMS, we need tighter timings
	if (menuConfig.sound.perfectSynchro && gblMachineType == EM_SMS)
		sample_count = 128;
	else
		sample_count = snd.sample_rate/snd.fps;
	sample_count = PSP_AUDIO_SAMPLE_ALIGN(sample_count);
	realSampleCount = sample_count / (44100 / snd.sample_rate);
	chan = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, sample_count, PSP_AUDIO_FORMAT_STEREO);

	if(chan > 0 && sample_count > 0 && sample_count <= 2048)
	{
		for(; soundRunning; )
		{
			if (soundPause)
				memset(audioOut[buf], 0, sample_count * sizeof(sample_t));
			else
				StandardSoundProcess();

			//Game Boy: réglage du son ne fonctionne pas!
			AudioGet(audioOut[buf], sample_count, realSampleCount);
			sceAudioOutputBlocking(chan, PSP_AUDIO_VOLUME_MAX, (void*)audioOut[buf]);

			if (menuConfig.sound.perfectSynchro)
				buf = 1 - buf;
		}

		sceAudioChRelease(chan);
	}

	soundRunning = -1;
	sceKernelExitDeleteThread(0);
}
