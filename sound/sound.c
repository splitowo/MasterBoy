/*
    sound.c --
    Sound emulation.
*/
#include "shared.h"

snd_t snd;
static int16 **fm_buffer;
static int16 **psg_buffer;
int *smptab;
int smptab_len;
int sound_justChange = 0;

void sound_change()		{
	sound_justChange = 1;
	sound_init();
	sound_justChange = 0;
}

int sound_init(void)
{
    uint8 *buf = NULL;
    int restore_fm = 0;
    int i;

    /* Save register settings */
	if (gblMachineType == EM_SMS)		{
		if(snd.enabled && sms.use_fm)
		{
			restore_fm = 1;
			buf = malloc(FM_GetContextSize());
			FM_GetContext(buf);
		}
	}

    /* If we are reinitializing, shut down sound emulation */
    if(snd.enabled)
    {
        sound_shutdown();
    }

    /* Disable sound until initialization is complete */
    snd.enabled = 0;

    /* Check if sample rate is invalid */
    if(snd.sample_rate < 8000 || snd.sample_rate > 48000)
        return 0;

    /* Assign stream mixing callback if none provided */
    if(!snd.mixer_callback)
        snd.mixer_callback = sound_mixer_callback;

    /* Calculate number of samples generated per frame */
    snd.sample_count = (snd.sample_rate / snd.fps);

    /* Calculate size of sample buffer */
    snd.buffer_size = snd.sample_count * 2;

	/* Allocate sound output streams */
	snd.output = malloc(snd.buffer_size * 2);
	if(!snd.output) return 0;

		/* Free sample buffer position table if previously allocated */
		if(smptab)
		{
			free(smptab);
			smptab = NULL;
		}

	/* Prepare incremental info */
	snd.done_so_far = 0;
	if (gblMachineType == EM_SMS)
		smptab_len = (sms.display == DISPLAY_NTSC) ? 262 : 313;
	else if (gblMachineType == EM_GBC)
		smptab_len = 154;
	smptab = malloc(smptab_len * sizeof(int));
	if(!smptab) return 0;
	for (i = 0; i < smptab_len; i++)
	{
		double calc = (snd.sample_count * i);
		calc = calc / (double)smptab_len;
		smptab[i] = (int)calc;
	}

	if (gblMachineType == EM_SMS)		{
		/* Allocate emulated sound streams */
		for(i = 0; i < STREAM_MAX; i++)
		{
			snd.stream[i] = malloc(snd.buffer_size);
			if(!snd.stream[i]) return 0;
			memset(snd.stream[i], 0, snd.buffer_size);
		}

		/* Set up buffer pointers */
		fm_buffer = (int16 **)&snd.stream[STREAM_FM_MO];
		psg_buffer = (int16 **)&snd.stream[STREAM_PSG_L];

		/* Set up SN76489 emulation */
		SN76489_Init(0, snd.psg_clock, snd.sample_rate);

		/* Set up YM2413 emulation */
		FM_Init();

		/* Inform other functions that we can use sound */
		snd.enabled = 1;

		/* Restore YM2413 register settings */
		if(restore_fm)
		{
			FM_SetContext(buf);
		}
	}

	snd.enabled = 1;
    return 1;
}


void sound_shutdown(void)
{
    int i;

    if (!snd.enabled)
        return;

    /* Free sound output buffers */
    if(snd.output)
    {
        free(snd.output);
        snd.output = NULL;
    }

	if (gblMachineType == EM_SMS)		{
		/* Free emulated sound streams */
		for(i = 0; i < STREAM_MAX; i++)
		{
			if(snd.stream[i])
			{
				free(snd.stream[i]);
				snd.stream[i] = NULL;
			}
		}

		/* Shut down SN76489 emulation */
		SN76489_Shutdown();

		/* Shut down YM2413 emulation */
		FM_Shutdown();
	}
}


void sound_reset(void)
{
    if(!snd.enabled)
        return;

    /* Reset SN76489 emulator */
    SN76489_Reset(0);

    /* Reset YM2413 emulator */
    FM_Reset();
}


void sound_update(int line)
{
    int16 *fm[2], *psg[2];
	
    if(!snd.enabled)
        return;

    /* Finish buffers at end of frame */
    if(line == smptab_len - 1)
    {
    	psg[0] = psg_buffer[0] + snd.done_so_far;
    	psg[1] = psg_buffer[1] + snd.done_so_far;
/*    	fm[0]  = fm_buffer[0] + snd.done_so_far;
    	fm[1]  = fm_buffer[1] + snd.done_so_far;*/

	/* Generate SN76489 sample data */
	SN76489_Update(0, psg, snd.sample_count - snd.done_so_far);

	/* Generate YM2413 sample data */
//	FM_Update(fm, snd.sample_count - snd.done_so_far);

	/* Mix streams into output buffer */
    	snd.mixer_callback(snd.stream, snd.output, snd.sample_count);
    	
    	/* Reset */
    	snd.done_so_far = 0;
    }
    else
    {
    	int tinybit;
    	
    	tinybit = smptab[line] - snd.done_so_far;
    	
        /* Do a tiny bit */
    	psg[0] = psg_buffer[0] + snd.done_so_far;
    	psg[1] = psg_buffer[1] + snd.done_so_far;
    	fm[0]  = fm_buffer[0] + snd.done_so_far;
    	fm[1]  = fm_buffer[1] + snd.done_so_far;

	    /* Generate SN76489 sample data */
	    SN76489_Update(0, psg, tinybit);

	    /* Generate YM2413 sample data */
//	    FM_Update(fm, tinybit);

		/* Sum total */
    	snd.done_so_far += tinybit;
    }
}

void sound_update_gb(int line)
{
    if(!snd.enabled)
        return;

    /* Finish buffers at end of frame */
    if(line == smptab_len - 1)
    {
		snd_render_orig(snd.output + snd.done_so_far, snd.sample_count - snd.done_so_far);

    	/* Reset */
    	snd.done_so_far = 0;
    }
    else
    {
    	int tinybit;
    	
    	tinybit = smptab[line] - snd.done_so_far;
    	
		if (tinybit > 0)
			snd_render_orig(snd.output + snd.done_so_far, tinybit);

		/* Sum total */
    	snd.done_so_far += tinybit;
    }
}

#if 1
void mix(u16 *out, u16 *psg0, u16 *psg1, int length)		{
    int i;
    for(i = 0; i < length; i++)
    {
        *out++ = psg0[i] << menuConfig.sound.volume;
        *out++ = psg1[i] << menuConfig.sound.volume;
    }
}

void sound_update_ex(void *outBuffer, int sampleCount)
{
    int16 *psg[2];
	int16 *temp_buf;
	
    if(!snd.enabled)
        return;

    temp_buf = malloc(snd.buffer_size * 2 * 2);

    /* Do a tiny bit */
	psg[0] = temp_buf;
	psg[1] = temp_buf + snd.buffer_size;

    /* Generate SN76489 sample data */
    SN76489_Update(0, psg, sampleCount);

    /* Generate YM2413 sample data */
//    FM_Update(fm, sampleCount);

	mix(outBuffer, psg[0], psg[1], sampleCount);
//	snd.mixer_callback(snd.stream, outBuffer, sampleCount);

	free(temp_buf);
}
#endif


/* Generic FM+PSG stereo mixer callback */
void sound_mixer_callback(int16 **stream, int16 *output, int length)
{
    int i, j = 0;
    for(i = 0; i < length; i++)
    {
//        int16 temp = (fm_buffer[0][i] + fm_buffer[1][i]) >> 1;
        output[j++] = psg_buffer[0][i] << menuConfig.sound.volume;
        output[j++] = psg_buffer[1][i] << menuConfig.sound.volume;
    }
}


/*--------------------------------------------------------------------------*/
/* Sound chip access handlers                                               */
/*--------------------------------------------------------------------------*/

void psg_stereo_w(int data)
{
    if(!snd.enabled)
        return;
    SN76489_GGStereoWrite(0, data);
}

void stream_update(int which, int position)
{
}


void psg_write(int data)
{
    if(!snd.enabled)
        return;
    SN76489_Write(0, data);
}

/*--------------------------------------------------------------------------*/
/* Mark III FM Unit / Master System (J) built-in FM handlers                */
/*--------------------------------------------------------------------------*/

int fmunit_detect_r(void)
{
    return sms.fm_detect;
}

void fmunit_detect_w(int data)
{
    sms.fm_detect = data;
}

void fmunit_write(int offset, int data)
{
    if(!snd.enabled || !sms.use_fm)
        return;

    FM_Write(offset, data);
}





