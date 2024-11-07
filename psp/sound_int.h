#include <oslib/oslib.h>
#include "sound.h"

typedef s64 __int64;
#define min(x,y)		(((x) < (y)) ? (x) : (y))
#define max(x,y)		(((x) > (y)) ? (x) : (y))

template<class T> class mem_block_t		{
private:
	T *ptr;
	int size;

public:
	mem_block_t()		{
		ptr = NULL;
		size = 0;
	}

	~mem_block_t()		{
		if (ptr)
			free(ptr);
	}

	void *get_ptr()		{
		return ptr;
	}

	int get_size()		{
		return size;
	}

	int set_size(int new_size)		{
		size = new_size;
		if (size == 0)		{
			if (ptr)
				free(ptr);
			ptr = NULL;
		}
		else	{
			T *ptrTemp = (T*)realloc(ptr, new_size);
			if (ptrTemp)
				ptr = ptrTemp;
			else
				return 0;
		}
		return 1;
	}
	
	int new_block(int size)		{
		//Don't keep original data
		this->set_size(0);
		return this->set_size(size);
	}
};

class SONGINFO			{
private:
	char *fname;
	float length;
public:

	SONGINFO()		{
		fname = new char[128];
	}

	~SONGINFO()			{
		delete fname;
	}

	void set_length(float new_length)		{
		length = new_length;
	}

	char *get_filename()		{
		return fname;
	}

	void set_filename(char *_file)		{
		strcpy(fname, _file);
	}

	void meta_add_ansi(char *str1, char *str2)		{
		if (!strcmp(str1, "TITLE"))
			strcpy(menuSoundTitle, str2);
		if (!strcmp(str1, "PUBLISHER"))
			strcpy(menuSoundAlbum, str2);
		if (!strcmp(str1, "ALBUM"))
			strcpy(menuSoundArtist, str2);
	}

	void info_set(char *str1, char *str2)		{
	}
};

/*class CFILE			{
private:
	FILE *file;
	int length, offset, type;
	void *memblock;
	enum {VF_MEMORY=1, VF_FILE=2};
	int blockfree;

public:
	CFILE()		{
		file = NULL;
		length = -1;
		memblock = NULL;
		blockfree = 0;
	}

	~CFILE()		{
		if (file)
			fclose(file);
		if (blockfree)
			free(memblock);
	}

	char open(const char *fname, const char *mode)		{
		if (file)
			fclose(file);
		file = fopen(fname, mode);
		memblock = NULL;
		length = -1;
		type = VF_FILE;
		offset = 0;
		return (file != NULL);
	}

	//Open from memory
	char open(void *memory, int size, const char *mode)		{
		if (file)
			fclose(file);
		file = NULL;
		memblock = memory;
		length = size;
		type = VF_MEMORY;
		offset = 0;
		return true;
	}

	u32 get_length()			{
		if (length == -1)		{
			if (file)		{
				int cur = ftell(file);
				int start, end;
				fseek(file, 0, SEEK_SET);
				start = ftell(file);
				fseek(file, 0, SEEK_END);
				end = ftell(file);
				fseek(file, cur, SEEK_SET);
				length = end - start;
			}
			else
				oslDebug("BUG");
		}
		return length;
	}

	int read(void *ptr, int size, int n=1)			{
		int readSize, realSize = size * n;
		if (memblock)		{
			//min => pour éviter les débordements
			readSize = min(realSize, length - offset);
			memcpy(ptr, (char*)memblock + offset, readSize);
			offset += realSize;
		}
		else if (file)
			readSize = fread(ptr, n, size, file);
		return readSize;
//		return fread(ptr, 1, size, f);
	}

	int readc()		{
		if (memblock)		{
			int c;
			c = *((char*)memblock + offset);
			return c;
		}
		else if (file)
			return fgetc(file);
	}

	int seek(int _offset)		{
		return seek(_offset, SEEK_SET);
	}

	int seek(int _offset, int location)		{
		if (memblock)		{
			if (location == SEEK_SET)
				offset = _offset;
			else if (location == SEEK_CUR)
				offset += _offset;
			else if (location == SEEK_END)
				offset = length - _offset;
			offset = max(min(offset, length), 0);
		}
		else if (file)
			return fseek(file, offset, location);
	}

	int unreadc(int car)		{
		if (file)
			return ungetc(car, file);
		else if (memblock)			{
			if (car != (int)*((char*)memblock + offset - 1))
				oslDebug("BUG");
			offset--;
		}
	}

	char tomemory()		{
		if (file)		{
			int size = get_length();
			void *block;
			if (size > 0)			{
				block = malloc(size);
				if (block)		{
					read(block, size);
					open(block, size, "rb");
					//On devra libérer le bloc
					blockfree = 1;
				}
			}
		}
		return false;
	}
};*/

class CFILE			{
private:
	FILE *file;
	int length, offset, type;
	void *memblock;
	enum {VF_MEMORY=1, VF_FILE=2};

public:
	char name[1024];
	int blockfree;

	CFILE()		{
		file = NULL;
		length = -1;
		memblock = NULL;
		blockfree = 0;
	}

	~CFILE()		{
		if (file)
			fclose(file);
		if (blockfree)
			free(memblock);
	}

	char open(const char *fname, const char *mode)		{
		if (file)
			fclose(file);
		if (blockfree)
			free(memblock);
		file = fopen(fname, mode);
		memblock = NULL;
		length = -1;
		type = VF_FILE;
		offset = 0;
		blockfree = 0;
		return (file != NULL);
	}

	//Open from memory
	char open(void *memory, int size, const char *mode)		{
		if (file)
			fclose(file);
		if (blockfree && memblock != memory)
			free(memblock);
		file = NULL;
		memblock = memory;
		length = size;
		type = VF_MEMORY;
		offset = 0;
		blockfree = 0;
		return true;
	}

	u32 get_length()			{
		if (length == -1)		{
			if (file)		{
				int cur = ftell(file);
				int start, end;
				fseek(file, 0, SEEK_SET);
				start = ftell(file);
				fseek(file, 0, SEEK_END);
				end = ftell(file);
				fseek(file, cur, SEEK_SET);
				length = end - start;
			}
//			else
//				oslDebug("BUG");
		}
		return length;
	}

	int read(void *ptr, int size, int n=1)			{
		int readSize, realSize = size * n;
		if (memblock)		{
			//min => pour éviter les débordements
			readSize = min(realSize, length - offset);
			memcpy(ptr, (char*)memblock + offset, readSize);
			offset += readSize;
		}
		else if (file)
			readSize = fread(ptr, n, size, file);
		return readSize;
//		return fread(ptr, 1, size, f);
	}

	int readc()		{
		if (memblock)		{
			int c;
			c = *((char*)memblock + offset);
			offset++;
			return c;
		}
		else
			return fgetc(file);
	}

	int seek(int _offset)		{
		return seek(_offset, SEEK_SET);
	}

	int seek(int _offset, int location)		{
		if (memblock)		{
			if (location == SEEK_SET)
				offset = _offset;
			else if (location == SEEK_CUR)
				offset += _offset;
			else if (location == SEEK_END)
				offset = length - _offset;
			offset = max(min(offset, length), 0);
		}
		else if (file)
			return fseek(file, _offset, location);
		return 0;
	}

	int unreadc(int car)		{
		if (file)
			return ungetc(car, file);
		else if (memblock)			{
//			if (car != (int)*((char*)memblock + offset - 1))
//				oslDebug("BUG");
			offset--;
		}
		return car;
	}

	char tomemory()		{
		if (file)		{
			int size = get_length();
			void *block;
			if (size > 0)			{
				block = malloc(size);
				if (block)		{
					read(block, size);
					open(block, size, "rb");
					//On devra libérer le bloc
					blockfree = 1;
				}
			}
		}
		return false;
	}

	void set_name(char *filename)		{
		strcpy(name, filename);
	}
};
