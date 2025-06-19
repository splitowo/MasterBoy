#include <oslib/oslib.h>
#include "VirtualGz.h"
#include <zlib.h>

/*
	SOURCE VFS: gzio
*/

int VF_GZFILE = -1;

#define _file_			((gzFile)f->ioPtr)

int vfsGzFileOpen(void *param1, int param2, int type, int mode, VIRTUAL_FILE* f)			{
	const char *stdMode = NULL;
	if (mode == VF_O_READ)
		stdMode = "r";
	else if (mode == VF_O_WRITE)
		stdMode = "w";
	else // if (mode == VF_O_READWRITE)
		return 0;
	
	f->ioPtr = (void*)gzopen((char*)param1, stdMode);
	if (mode == VF_O_WRITE && !f->ioPtr)		{
		//Create the path if it doesn't exist
		char path[1024];
		extractFilePath(param1, path, 1);
		makeDir(path);
		f->ioPtr = (void*)gzopen((char*)param1, stdMode);
	}

	return (int)f->ioPtr;
}

int vfsGzFileClose(VIRTUAL_FILE *f)				{
	gzclose(_file_);
	return 1;
}

int vfsGzFileWrite(const void *ptr, size_t size, size_t n, VIRTUAL_FILE* f)			{
	return gzwrite(_file_, ptr, size * n);
}

int vfsGzFileRead(void *ptr, size_t size, size_t n, VIRTUAL_FILE* f)			{
	int readSize = gzread(_file_, ptr, size * n);
	return readSize;
}

int vfsGzFileGetc(VIRTUAL_FILE *f)		{
/*	unsigned char car = -1;
	sceIoRead(_file_, &car, sizeof(car));
	return (int)car;*/
	return vfsMemGetc(f);
}

int vfsGzFilePutc(int caractere, VIRTUAL_FILE *f)		{
/*	unsigned char car = (unsigned char)caractere;
	return sceIoWrite(_file_, &car, sizeof(car));*/
	return vfsMemPutc(caractere, f);
}

char *vfsGzFileGets(char *str, int maxLen, VIRTUAL_FILE *f)			{
	return vfsMemGets(str, maxLen, f);
}

void vfsGzFilePuts(const char *s, VIRTUAL_FILE *f)		{
	return vfsMemPuts(s, f);
}

void vfsGzFileSeek(VIRTUAL_FILE *f, int offset, int whence)		{
//	int oldOffset = sceIoLseek32(_file_, 0, SEEK_CUR);
//	if (!(offset == 0 && whence == SEEK_CUR))
//		sceIoLseek32(_file_, offset, whence);
	gzseek(_file_, offset, whence);
}

int vfsGzFileTell(VIRTUAL_FILE *f)		{
	return gztell(_file_);
}

int vfsGzFileEof(VIRTUAL_FILE *f)		{
	return gzeof(_file_);
}

VIRTUAL_FILE_SOURCE vfsGzFile =		{
	vfsGzFileOpen,
	vfsGzFileClose,
	vfsGzFileRead,
	vfsGzFileWrite,
	vfsGzFileGetc,
	vfsGzFilePutc,
	vfsGzFileGets,
	vfsGzFilePuts,
	vfsGzFileSeek,
	vfsGzFileTell,
	vfsGzFileEof,
};

int vfsGzFileInit()		{
	if (VF_GZFILE < 0)
		VF_GZFILE = VirtualFileRegisterSource(&vfsGzFile);
	return VF_GZFILE;
}



