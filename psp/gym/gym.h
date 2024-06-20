#ifndef GYM_H
#define GYM_H

	#define double float
	#define fnum(flo)		flo##f

	typedef unsigned char BYTE;

	namespace CODEC_GYM			{
	struct GYMTAG
	{
		char gym_id[4];
		char song_title[32];
		char game_title[32];
		char game_publisher[32];
		char dumper_emu[32];
		char dumper_person[32];
		char comments[256];
		unsigned int looped;
		unsigned int compressed;
	};

	class input_gym
	{
	private:
		mem_block_t<BYTE> gym_backup;
		mem_block_t<unsigned short> sample_buffer;

		unsigned char *gym_pos;
		unsigned char *gym_start;
		char gym_loaded;
		char gym_compressed;
		int gym_size;
		int gym_loop;
		double song_length;
		double loop_length;
		double gym_length;
		double gym_length_with_fade;
		double gym_pos_ms;

		GYMTAG *gym_tag;

		int playingSampleRate;

		int songLen;
		int fadeLen;

		int load_ezpk_gym(void *gymFile);
		int load_zlib_gym(void *gymFile);
		void calc_gym_time_length(SONGINFO* info);
		
		virtual char is_our_content_type(const char * url,const char * type)
		{
			return !strcmp(type,"audio/gym");
		}

		virtual char test_filename(const char * fn,const char * ext) 
		{
			return !strcasecmp(ext,"GYM");
		}
		int open_file(CFILE *r, SONGINFO *info, unsigned int flags);

	public:
		input_gym();
		~input_gym();
		void construct();
		void destruct();
		void reset();
		void set_frequency(int newFrequency);
		virtual char open(CFILE * r, SONGINFO * info, unsigned int flags);
		virtual int get_samples_pcm(void ** buffer,int * size,int * srate,int * bps,int * nch);
		virtual char seek (double seconds);
	};
	}
#endif
