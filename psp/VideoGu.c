
#include <pspdisplay.h>
#include <pspgu.h>
#include "shared.h"
#include "VideoGu.h"
#include "pspcommon.h"

//uint32 pspGuList [262144] __attribute__((aligned(16)));
screen_t Screen, ScreenTemp;
int videoInited = 0;
//char bVframeUncacheNeeded = 1;

#define ALTERNATE_FILLING
//Défini dans VIDEOGU.H (utilisé dans menuplus.c aussi!)
//#define SKIP_BORDERS

struct Vertex
{
  unsigned short u, v;
  short x, y, z;
};

struct VertexF
{
  float u, v;
  short x, y, z;
};


#ifdef ALTERNATE_FILLING

#define HEIGHT 272
#define WIDTH 480

struct VertexUntex
{
  unsigned long color;
  short x, y, z;
};
#endif

void VideoGuInit(void)
{
/*	sceGuInit();

	sceGuStart(0, pspGuList);
	sceGuDrawBufferList(GU_PSM_5551, (void *)0, 512);
	sceGuDispBuffer(480, 272, (void *)0x88000, 512);
	sceGuDepthBuffer((void *)0x110000, 512);
	sceGuOffset(2048 - (480/2), 2048 - (272/2));
	sceGuViewport(2048, 2048, 480, 272);
	sceGuDepthRange(0xc350, 0x2710);
	sceGuScissor(0, 0, 480, 272);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuFrontFace(GU_CW);
	sceGuEnable(GU_TEXTURE_2D);
	sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
	sceGuFinish();

	sceGuSync(0, 0);
	sceDisplayWaitVblankStart();
	sceGuDisplay(1);*/

	if (!videoInited)		{
		oslInitGfx(OSL_PF_5650, 1);
		oslInitConsole();
		oslStartDrawing();
		oslEndDrawing();
		oslShowSplashScreen(2);
		oslShowSplashScreen(1);
		VideoGuScreenClear();

		//Main screen: in VRAM for speed
		Screen.scrBuffer = oslGetUncachedPtr(oslVramMgrAllocBlock(SCR_BUFFER_SIZE));
		Screen.pal = oslGetUncachedPtr(oslVramMgrAllocBlock(256*2));
		//Sub screen: in RAM for mem sparing
		ScreenTemp.scrBuffer = oslGetUncachedPtr(malloc(SCR_BUFFER_SIZE));
		ScreenTemp.pal = NULL;

		bitmap.data   = (unsigned char*)Screen.scrBuffer;
		videoInited = 1;
	}
}

void VideoGuTerm(void)
{
	sceGuTerm();
}

void DisplayBenchmark()			{
	if (menuConfig.video.cpuTime == 1)		{
		int ms6;
		ms6 = oslMeanBenchmarkTestEx(OSL_BENCH_GET, 6);
		oslPrintf_xy(0,0, "%i.%i%%", (ms6 * 6) /1000, ((ms6 * 6) % 1000) / 100);
	}
	else if (menuConfig.video.cpuTime == 2)		{
		oslPrintf_xy(0,0, "%i fps (%i%%)", gblFramerate, 100 * gblVirtualFramerate / snd.fps);
	}
}

//mustClear = -1: ne surtout pas clearer!
void VideoGuUpdate_Core(int fullScreen, int mustClear)		{
	int viewPortX = bitmap.viewport.x, viewPortW = bitmap.viewport.w;
	static int currentX = 0;
#ifdef SKIP_BORDERS
	int cutBorders = (menuConfig.video.smoothing);
#else
	int cutBorders = 0;
#endif

	if (bitmap.viewport.x == 0)		{
		int target = 0;
		//Auto detect the left bar
		if (((vdp.reg[0] & 0x20) && menuConfig.video.leftBar == 2) || menuConfig.video.leftBar == 0)
			target = 8;
		if (currentX > target)		{
			currentX--;
			mustClear = 1;
		}
		else if (currentX < target)			{
			currentX++;
			mustClear = 1;
		}
		bitmap.viewport.x = currentX;
		bitmap.viewport.w -= currentX - viewPortX;
	}

	//1x
	if(fullScreen == 0)
	{
		int x = (480 - bitmap.viewport.w) / 2, y = (272 - bitmap.viewport.h) / 2;
#ifdef SKIP_BORDERS
		int add = cutBorders ? 1 : 0;
		pspScaleImage((char *)Screen.scrBuffer, 
						bitmap.viewport.x,
						bitmap.viewport.y,
						bitmap.viewport.x + bitmap.viewport.w,
						bitmap.viewport.y + bitmap.viewport.h ,
						256,
						x, y, x + bitmap.viewport.w - add, y + bitmap.viewport.h - add, mustClear, cutBorders);
#else
		pspScaleImage((char *)Screen.scrBuffer, 
						bitmap.viewport.x,
						bitmap.viewport.y,
						bitmap.viewport.x + bitmap.viewport.w,
						bitmap.viewport.y + bitmap.viewport.h,
						256,
						x, y, x + bitmap.viewport.w, y + bitmap.viewport.h, mustClear, cutBorders);
#endif
	} 
	//2x
	else if (fullScreen == 1)
	{
		int x = 480 / 2 - bitmap.viewport.w, y = 272 / 2 - bitmap.viewport.h;
		pspScaleImage((char *)Screen.scrBuffer, 
						bitmap.viewport.x,
						bitmap.viewport.y,
						bitmap.viewport.x + bitmap.viewport.w,
						bitmap.viewport.y + bitmap.viewport.h,
						256,
						x, y, x + bitmap.viewport.w * 2, y + bitmap.viewport.h * 2, mustClear, cutBorders);
	}
	//Fit
	else if (fullScreen == 2)
	{
		int scale = min((4096 * 480) / bitmap.viewport.w, (4096 * 272) / bitmap.viewport.h);
		int x = 480 / 2 - (((bitmap.viewport.w * scale) >> 12) / 2),
			y = 272 / 2 - (((bitmap.viewport.h * scale) >> 12) / 2);
		pspScaleImage((char *)Screen.scrBuffer, 
						bitmap.viewport.x,
						bitmap.viewport.y,
						bitmap.viewport.x + bitmap.viewport.w,
						bitmap.viewport.y + bitmap.viewport.h,
						256,
						x, y, x + ((bitmap.viewport.w * scale) >> 12), y + ((bitmap.viewport.h * scale) >> 12),
						mustClear, cutBorders);
	}
	//Wide
	else if (fullScreen == 3)
	{
		int scaleY = min((4096 * 480) / bitmap.viewport.w, (4096 * 272) / bitmap.viewport.h);
		int scaleX = (scaleY + (4096 * 480) / bitmap.viewport.w) / 2;
		int x = 480 / 2 - (((bitmap.viewport.w * scaleX) >> 12) / 2),
			y = 272 / 2 - (((bitmap.viewport.h * scaleY) >> 12) / 2);
		pspScaleImage((char *)Screen.scrBuffer, 
						bitmap.viewport.x,
						bitmap.viewport.y,
						bitmap.viewport.x + bitmap.viewport.w,
						bitmap.viewport.y + bitmap.viewport.h,
						256,
						x, y, x + ((bitmap.viewport.w * scaleX) >> 12), y + ((bitmap.viewport.h * scaleY) >> 12),
						mustClear, cutBorders);
	}
	//1.5x
	else if (fullScreen == 5)
	{
		int x = 480 / 2 - (3 * bitmap.viewport.w) / 4, y = 272 / 2 - (3 * bitmap.viewport.h) / 4;
		pspScaleImage((char *)Screen.scrBuffer, 
						bitmap.viewport.x,
						bitmap.viewport.y,
						bitmap.viewport.x + bitmap.viewport.w,
						bitmap.viewport.y + bitmap.viewport.h,
						256,
						x, y, x + (bitmap.viewport.w * 3) / 2, y + (bitmap.viewport.h * 3) / 2,
						mustClear, cutBorders);
	}
	//Full
	else
	{
		pspScaleImage((char *)Screen.scrBuffer, 
						bitmap.viewport.x,
						bitmap.viewport.y,
						bitmap.viewport.x + bitmap.viewport.w,
						bitmap.viewport.y + bitmap.viewport.h,
						256,
						0, 0, 480, 272, mustClear, cutBorders);
	}
	bitmap.viewport.x = viewPortX;
	bitmap.viewport.w = viewPortW;
}

void VideoGuUpdate(int nFrame, int fullScreen)
{
	int mustClear = 0;

	oslStartDrawing();
	//Pour le texte
	oslSetTextColor(RGB(255,255,255));
	oslSetBkColor(RGBA(0,0,0,0x80));

	if (menuStatusDrawn > 0)
		mustClear = 1;

	//The first times, always clear the screen completely
	if (nFrame < 4)
		oslClearScreen(0), mustClear = 0;

	VideoGuUpdate_Core(fullScreen, mustClear);

	if (menuConfig.video.cpuTime)
		DisplayBenchmark();
	menuDrawStatusMessage();
//	oslEndDrawing();
}

void VideoGuScreenClear(void)
{
//	VideoGuTerm();
//	VideoGuInit();

	oslStartDrawing();
	oslClearScreen(RGB(0,0,0));
	oslEndDrawing();
	oslSwapBuffers();

	oslStartDrawing();
	oslClearScreen(RGB(0,0,0));
	oslEndDrawing();
	oslSwapBuffers();
}


#define MIN(a,b)        (((a)<(b))?(a):(b))
#define MAX(a,b)        (((a)>(b))?(a):(b))

void RGBToHSL(unsigned char r, unsigned char g, unsigned char b,
		 unsigned *h, unsigned *s, unsigned *l)
{
    unsigned int v, m, vm, r2, g2, b2;
    unsigned int h2, s2, l2;

    h2 = s2 = l2 = 0;

    v = MAX(r,g);
    v = MAX(v,b);
    m = MIN(r,g);
    m = MIN(m,b);

    if ((l2 = ((m + v) << 8) >> 1) == 0) goto done;
    if ((s2 = vm = (v - m)) == 0) goto done;
    else s2 = (s2 << 16) / ((l2 < 32768) ? (v+m) : (512 - v - m));

    r2 = ((((v-r) << 16) / vm) << 8) >> 16;
    g2 = ((((v-g) << 16) / vm) << 8) >> 16;
    b2 = ((((v-b) << 16) / vm) << 8) >> 16;

    if (r == v)
	h2 = (g == m ? 1280 + b2 : 256 - g2);
    else if (g == v)
	h2 = (b == m ? 256 + r2 : 768 - b2);
    else
	h2 = (r == m ? 768 + g2 : 1280 - r2);

 done:
    *h = h2;
    *s = s2;
    *l = l2;
}

/*
 * 0 <= h <= 359
 * 0 <= s,l <= 100
 * 0 <= r,g,b <= 65535
 */
void HSLToRGB(unsigned h, unsigned s, unsigned l,
		 unsigned char *r, unsigned char *g, unsigned char *b)
{
    int v;
    int m, sv, fract, vsf, mid1, mid2, sextant;

    v = (l < 32768) ? (l * (s + 65536) >> 16) : (l + s - ((l * s) >> 16));
    if (v <= 0) { *r = *g = *b = 0; return;}

    m = l + l - v;
    sv = (((v - m) << 8) / v);

    sextant = h >> 8;
    fract = ((h - (sextant << 8)) << 8) >> 8;
    vsf = (v * sv * fract) >> 16;
    mid1 = m + vsf;
    mid2 = v - vsf;

	v >>= 8;
	m >>= 8;
	mid1 >>= 8;
	mid2 >>= 8;

    switch (sextant)			{
		case 6:
		case 0:
			*r = v; *g = mid1; *b = m; break;
		case 1:
			*r = mid2; *g = v; *b = m; break;
		case 2:
			*r = m; *g = v; *b = mid1; break;
		case 3:
			*r = m; *g = mid2; *b = v; break;
		case 4:
			*r = mid1; *g = m; *b = v; break;
		case 5:
			*r = v; *g = m; *b = mid2; break;
    }
}


uint16 MAKE_COLOR(uint8 r, uint8 g, uint8 b)		{
	if (menuConfig.video.vibrance > 128)			{
		unsigned h, s, l;
		unsigned coeff2 = ((256 - menuConfig.video.vibrance) >> 1) + 64;
		RGBToHSL(r, g, b, &h, &s, &l);
		//We can't do anything with a non saturated color
		if (s)		{
			s = 65536 - (((65536 - s) * (256 - menuConfig.video.vibrance)) >> 7);
			if (l > 32768)
				l = (((l - 32768) * coeff2) >> 7) + 32768;
			else
				l = 32768 - (((32768 - l) * coeff2) >> 7);
		}
		HSLToRGB(h, s, l, &r, &g, &b);

/*		unsigned h, s, l;
		unsigned coeff2 = (menuConfig.video.vibrance - 128) << 9;
		RGBToHSL(r, g, b, &h, &s, &l);
		//We can't do anything with a non saturated color
		if (s)		{
			s += coeff2;
			if (s > 65530)
				s = 65530;
			if (l > 32768)			{
				l -= coeff2 >> 2;
				if (l < 32768)
					l = 32768;
			}
			else		{
				l += coeff2 >> 2;
				if (l > 32768)
					l = 32768;
			}
		}
		HSLToRGB(h, s, l, &r, &g, &b);*/
	}
	else if (menuConfig.video.vibrance < 128)			{
		int value = (b * 77 + r * 84 + g * 95) >> 8;
		int coeff = menuConfig.video.vibrance;
		r = (coeff * r + (127 - coeff) * value) >> 7;
		g = (coeff * g + (127 - coeff) * value) >> 7;
		b = (coeff * b + (127 - coeff) * value) >> 7;
	}
	return MAKE_PIXEL(r, g, b);
}


void RecolorPaletteVibrance()		{
	unsigned h, s, l;
	uint8 r, g, b;
	int i;
	int coeff, coeff2;
	
	coeff = 256 - menuConfig.video.vibrance;
	coeff2 = (256 - menuConfig.video.vibrance) / 2 + 64;

	for(i=0; i<PALETTE_SIZE; i++)
	{
		if (bitmap.pal.dirty[i])			{
			r = bitmap.pal.color[i][0];
			g = bitmap.pal.color[i][1];
			b = bitmap.pal.color[i][2];

			RGBToHSL(r, g, b, &h, &s, &l);
			//We can't do anything with a non saturated color
			if (s)		{
/*				int sextant = h & 255;
				if (sextant < 128)
					//On divise le coeff par 4
					h -= (sextant * coeff) >> 9;
				else
					h += ((255 - sextant) * coeff) >> 9;*/
				s = 65536 - (((65536 - s) * coeff) >> 7);
				if (l > 32768)
					l = (((l - 32768) * coeff2) >> 7) + 32768;
				else
					l = 32768 - (((32768 - l) * coeff2) >> 7);
			}
			HSLToRGB(h, s, l, &r, &g, &b);

			Screen.pal[i] = MAKE_PIXEL(r,g,b);
			Screen.pal[i+PALETTE_SIZE] = MAKE_PIXEL(r,g,b);
			Screen.pal[i+(2*PALETTE_SIZE)] = MAKE_PIXEL(r,g,b);
			Screen.pal[i+(3*PALETTE_SIZE)] = MAKE_PIXEL(r,g,b);
		}
	}
}

void RecolorPaletteGray()		{
	uint8 r, g, b;
	int i;
	int coeff, value;
	
	coeff = menuConfig.video.vibrance;

	for(i=0; i<PALETTE_SIZE; i++)
	{
		if (bitmap.pal.dirty[i])			{
			r = bitmap.pal.color[i][0];
			g = bitmap.pal.color[i][1];
			b = bitmap.pal.color[i][2];

			value = (b * 49 + r * 77 + g * 130) >> 8;
			r = (coeff * r + (127 - coeff) * value) >> 7;
			g = (coeff * g + (127 - coeff) * value) >> 7;
			b = (coeff * b + (127 - coeff) * value) >> 7;

			Screen.pal[i] = MAKE_PIXEL(r,g,b);
			Screen.pal[i+PALETTE_SIZE] = MAKE_PIXEL(r,g,b);
			Screen.pal[i+(2*PALETTE_SIZE)] = MAKE_PIXEL(r,g,b);
			Screen.pal[i+(3*PALETTE_SIZE)] = MAKE_PIXEL(r,g,b);
		}
	}
}


//Faster, but doesn't support the vibrance control effects
void RecolorPalette()		{
	uint8 r, g, b;
	int i;

	for(i=0; i<PALETTE_SIZE; i++)
	{
		if (bitmap.pal.dirty[i])			{
			r = bitmap.pal.color[i][0];
			g = bitmap.pal.color[i][1];
			b = bitmap.pal.color[i][2];

			Screen.pal[i] = MAKE_PIXEL(r,g,b);
			Screen.pal[i+PALETTE_SIZE] = MAKE_PIXEL(r,g,b);
			Screen.pal[i+(2*PALETTE_SIZE)] = MAKE_PIXEL(r,g,b);
			Screen.pal[i+(3*PALETTE_SIZE)] = MAKE_PIXEL(r,g,b);
		}
	}
}


#define SLICE_SIZE 64 // Change this to experiment with different page-cache sizes

//clear = -1: ne surtout pas clearer! 0: le choix, 1 = toujours
#ifdef SKIP_BORDERS
void pspScaleImage_core(char *pixels, int srcX0, int srcY0, int srcX1, int srcY1, 
				   int srcPitch, int dstX0, int dstY0, int dstX1, int dstY1, int clear, int cutborders)
#else
void pspScaleImage_core(char *pixels, int srcX0, int srcY0, int srcX1, int srcY1, 
				   int srcPitch, int dstX0, int dstY0, int dstX1, int dstY1, int clear)
#endif
{
	int x, i, j;
#ifdef SKIP_BORDERS
	struct VertexF* vertices;
#else
	struct Vertex* vertices;
#endif
	struct VertexUntex* verticesUntex;

	//Découpage des bords (0.5)
#ifdef SKIP_BORDERS
	float add;
	if (cutborders)
		srcX1--, srcY1--, add = 0.5f;
	else
		add = 0.0f;
#endif

	float slice_scale = (SLICE_SIZE * (dstX1 - dstX0) / (srcX1 - srcX0));

#ifndef ALTERNATE_FILLING
	if (clear == 1)
		oslClearScreen(0);
#endif

	if (bitmap.depth == 8)		{
		if(bitmap.pal.update)
		{
			if (menuConfig.video.vibrance == 128)
				RecolorPalette();
			else if (menuConfig.video.vibrance > 128)
				RecolorPaletteVibrance();
			else
				RecolorPaletteGray();
/*			for(i=0; i<PALETTE_SIZE; i++)
			{
				r = bitmap.pal.color[i][0];
				g = bitmap.pal.color[i][1];
				b = bitmap.pal.color[i][2];

				Screen.pal[i] = MAKE_PIXEL(r,g,b);
				Screen.pal[i+PALETTE_SIZE] = MAKE_PIXEL(r,g,b);
				Screen.pal[i+(2*PALETTE_SIZE)] = MAKE_PIXEL(r,g,b);
				Screen.pal[i+(3*PALETTE_SIZE)] = MAKE_PIXEL(r,g,b);
			}*/
		}

//		sceKernelDcacheWritebackInvalidateRange(Screen.pal, PALETTE_SIZE * 4 * 2);

		sceGuClutMode(GU_PSM_5551,0,0xff,0);
		sceGuClutLoad((256/8),Screen.pal);
		sceGuTexMode(GU_PSM_T8,0,0,0);

		//OSLib will have to upload the palette again
		osl_curPalette = NULL;
	}
	else		{
		//Should be done here but for a question of performance, I do it after having begin to draw (the upper part where the GPU begins is always uploaded)
//		sceKernelDcacheWritebackInvalidateRange(pixels, 256 * 256 * 2);
		sceGuTexMode(GU_PSM_5551,0,0,0);
	}
	
	sceGuTexImage(0,256,256,srcPitch,oslGetCachedPtr(pixels));
	//So that OSLib knows that the texture should be reloaded
	osl_curTexture = NULL;

	oslEnableTexturing();
	sceGuDisable(GU_DITHER);
	oslSetBilinearFilter(menuConfig.video.smoothing);
	// do a striped blit (takes the page-cache into account)
	x=dstX0;

	//On dessine d'abord le haut de l'écran: il faut le faire rapidement sinon un lcd_render ou autre viendra remplacer le buffer pendant qu'on dessine la dernière bande!
	#define FIRST_LINE_SIZE 16
	int totalSlices = ((srcX1 - srcX0 + SLICE_SIZE - 1) / SLICE_SIZE) * 2, k = 0;
	float slice_scaley = (FIRST_LINE_SIZE * (dstY1 - dstY0) / (srcY1 - srcY0));
#ifdef SKIP_BORDERS
	vertices = (struct VertexF*)sceGuGetMemory(2 * sizeof(struct VertexF) * totalSlices);
#else
	vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex) * totalSlices);
#endif

	for (j = srcX0; j < srcX1; j += SLICE_SIZE, x += slice_scale)
	{
		//On commence d'abord avec le haut pour des raisons de synchronisation
#ifdef SKIP_BORDERS
		vertices[k].u = j+add; vertices[k].v = srcY0+add;
#else
		vertices[k].u = j; vertices[k].v = srcY0;
#endif
		vertices[k].x = x; vertices[k].y = dstY0; vertices[k].z = 0;

		//Plus bas
#ifdef SKIP_BORDERS
		vertices[k+totalSlices].u = j+add; vertices[k+totalSlices].v = srcY0 + FIRST_LINE_SIZE + add;
#else
		vertices[k+totalSlices].u = j; vertices[k+totalSlices].v = srcY0 + FIRST_LINE_SIZE;
#endif
		vertices[k+totalSlices].x = x; vertices[k+totalSlices].y = dstY0+slice_scaley; vertices[k+totalSlices].z = 0;
		k++;

#ifdef SKIP_BORDERS
		vertices[k].u = min(j+SLICE_SIZE, srcX1) + add; vertices[k].v = srcY0 + FIRST_LINE_SIZE + add;
#else
		vertices[k].u = min(j+SLICE_SIZE, srcX1); vertices[k].v = srcY0 + FIRST_LINE_SIZE;
#endif
		vertices[k].x = min(x+slice_scale, dstX1); vertices[k].y = dstY0+slice_scaley; 
		vertices[k].z = 0;

		//Plus bas
#ifdef SKIP_BORDERS
		vertices[k+totalSlices].u = min(j+SLICE_SIZE, srcX1) + add; vertices[k+totalSlices].v = srcY1 + add;
#else
		vertices[k+totalSlices].u = min(j+SLICE_SIZE, srcX1); vertices[k+totalSlices].v = srcY1;
#endif
		vertices[k+totalSlices].x = min(x+slice_scale, dstX1); vertices[k+totalSlices].y = dstY1; 
		vertices[k+totalSlices].z = 0;
		k++;
	}

#ifdef SKIP_BORDERS
	sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_VERTEX_16BIT|GU_TRANSFORM_2D, 2 * totalSlices, 0, vertices);
#else
	sceGuDrawArray(GU_SPRITES,GU_TEXTURE_16BIT|GU_VERTEX_16BIT|GU_TRANSFORM_2D, 2 * totalSlices, 0, vertices);
#endif

/*	int totalSlices = ((srcX1 - srcX0 + SLICE_SIZE - 1) / SLICE_SIZE) * 2, k = 0;
	vertices = (struct Vertex*)sceGuGetMemory(sizeof(struct Vertex) * totalSlices);

	for (j = srcX0; j < srcX1; j += SLICE_SIZE, x += slice_scale)
	{
		//On commence d'abord avec le haut pour des raisons de synchronisation
		vertices[k].u = j; vertices[k].v = srcY0;
		vertices[k].x = x; vertices[k].y = dstY0; vertices[k].z = 0;
		k++;

		vertices[k].u = min(j+SLICE_SIZE, srcX1); vertices[k].v = srcY1;
		vertices[k].x = min(x+slice_scale, dstX1); vertices[k].y = dstY1; 
		vertices[k].z = 0;
		k++;
	}

	sceGuDrawArray(GU_SPRITES,GU_TEXTURE_16BIT|GU_VERTEX_16BIT|GU_TRANSFORM_2D, totalSlices, 0, vertices);*/

#ifdef ALTERNATE_FILLING
	if (clear != -1)			{
		int color = oslBlendColor(RGBA(0, 0, 0, 255));
		oslDisableTexturing();

		if (dstX0 > 1)		{
			//Bande latérale gauche
			verticesUntex = (struct VertexUntex*)sceGuGetMemory(4 * sizeof(*verticesUntex));
			verticesUntex[0].x = 0;
			verticesUntex[1].x = dstX0;
			verticesUntex[0].y = 0;
			verticesUntex[1].y = HEIGHT;
			verticesUntex[0].z = verticesUntex[1].z = 0;
			verticesUntex[0].color = verticesUntex[1].color = color;

			//Bande latérale droite
			verticesUntex[2].x = dstX1;
			verticesUntex[3].x = WIDTH;
			verticesUntex[2].y = 0;
			verticesUntex[3].y = HEIGHT;
			verticesUntex[2].z = verticesUntex[3].z = 0;
			verticesUntex[2].color = verticesUntex[3].color = color;

			sceGuDrawArray(GU_SPRITES, GU_COLOR_8888|GU_VERTEX_16BIT|GU_TRANSFORM_2D, 4, 0, verticesUntex);
		}

		if (dstY0 > 1)		{
			//Bande horizontale haut
			verticesUntex = (struct VertexUntex*)sceGuGetMemory(4 * sizeof(*verticesUntex));
			verticesUntex[0].x = 0;
			verticesUntex[1].x = WIDTH;
			verticesUntex[0].y = 0;
			verticesUntex[1].y = dstY0;
			verticesUntex[0].z = verticesUntex[1].z = 0;
			verticesUntex[0].color = verticesUntex[1].color = color;

			//Bande horizontale bas
			verticesUntex[2].x = 0;
			verticesUntex[3].x = WIDTH;
			verticesUntex[2].y = dstY1;
			verticesUntex[3].y = HEIGHT;
			verticesUntex[2].z = verticesUntex[3].z = 0;
			verticesUntex[2].color = verticesUntex[3].color = color;

			sceGuDrawArray(GU_SPRITES, GU_COLOR_8888|GU_VERTEX_16BIT|GU_TRANSFORM_2D, 4, 0, verticesUntex);
		}
	}
#endif

	oslSetBilinearFilter(0);

/*	if (bVframeUncacheNeeded)		{
		sceKernelDcacheWritebackInvalidateRange(pixels, (256 * 256 * bitmap.depth) >> 3);
		bVframeUncacheNeeded = 0;
	}*/
	return;
}
