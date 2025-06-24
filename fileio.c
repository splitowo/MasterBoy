/*
    fileio.c --
    File management.
*/

#include "shared.h"

//Retourne l'extension en incluant le '.'.
char *getFileNameExtension(char *nom_fichier)		{
	char *ptrPoint = nom_fichier;
	while(*nom_fichier)		{
		if (*nom_fichier == '.')
			ptrPoint = nom_fichier;
		nom_fichier++;
	}
	return ptrPoint;
}

uint8 *loadFromZipByName(char *archive, char *filename, int *filesize)
{
    char name[PATH_MAX];
    unsigned char *buffer;
	int i;
	const char *recognizedExtensions[] = {
		".bin",
		".gb",
		".sgb",
		".gbc",
		".cgb",
		".sms",
		".gg",
	};

    int zerror = UNZ_OK;
    unzFile zhandle;
    unz_file_info zinfo;

    zhandle = unzOpen(archive);
    if(!zhandle) return (NULL);

    /* Seek to first file in archive */
    zerror = unzGoToFirstFile(zhandle);
    if(zerror != UNZ_OK)
    {
        unzClose(zhandle);
        return (NULL);
    }

	//On scanne tous les fichiers de l'archive et ne prend que ceux qui ont une extension valable, sinon on prend le dernier fichier trouvé...
	while (zerror == UNZ_OK) {
		if (unzGetCurrentFileInfo(zhandle, &zinfo, name, 0xff, NULL, 0, NULL, 0) != UNZ_OK) {
			unzClose(zhandle);
			return NULL;
		}

		//Vérifions que c'est la bonne extension
		char *extension = getFileNameExtension(name);

		for (i=0;i<numberof(recognizedExtensions);i++)		{
			if (!strcmp(extension, recognizedExtensions[i]))
				break;
		}
		if (i < numberof(recognizedExtensions))
			break;

		zerror = unzGoToNextFile(zhandle);
	}

    /* Get information about the file */
//    unzGetCurrentFileInfo(zhandle, &zinfo, &name[0], 0xff, NULL, 0, NULL, 0);
    *filesize = zinfo.uncompressed_size;

    /* Error: file size is zero */
    if(*filesize <= 0)
    {
        unzClose(zhandle);
        return (NULL);
    }

    /* Open current file */
    zerror = unzOpenCurrentFile(zhandle);
    if(zerror != UNZ_OK)
    {
        unzClose(zhandle);
        return (NULL);
    }

    /* Allocate buffer and read in file */
    buffer = malloc(*filesize);
    if(!buffer) return (NULL);
    zerror = unzReadCurrentFile(zhandle, buffer, *filesize);

    /* Internal error: free buffer and close file */
    if(zerror < 0 || zerror != *filesize)
    {
        free(buffer);
        buffer = NULL;
        unzCloseCurrentFile(zhandle);
        unzClose(zhandle);
        return (NULL);
    }

    /* Close current file and archive file */
    unzCloseCurrentFile(zhandle);
    unzClose(zhandle);

    memcpy(filename, name, PATH_MAX);
    return (buffer);
}

/*
    Verifies if a file is a ZIP archive or not.
    Returns: 1= ZIP archive, 0= not a ZIP archive
*/
int check_zip(char *filename)
{
    uint8 buf[2];
    FILE *fd = NULL;
    fd = fopen(filename, "rb");
    if(!fd) return (0);
    fread(buf, 2, 1, fd);
    fclose(fd);
    if(memcmp(buf, "PK", 2) == 0) return (1);
    return (0);
}


/*
    Returns the size of a GZ compressed file.
*/
int gzsize(gzFile gd)
{
    #define CHUNKSIZE   (0x10000)
    int size = 0, length = 0;
    unsigned char buffer[CHUNKSIZE];
    gzrewind(gd);
    do {
        size = gzread(gd, buffer, CHUNKSIZE);
        if(size <= 0) break;
        length += size;
    } while (!gzeof(gd));
    gzrewind(gd);
    return (length);
    #undef CHUNKSIZE
}




