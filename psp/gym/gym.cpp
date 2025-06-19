/*#include "../SDK/input.h"
#include "../SDK/config.h"
#include "../SDK/console.h"
#include "../SDK/componentversion.h"
#include "../SDK/tagread.h"*/

#include "gym_play.h"
#include "ym2612.h"
#include "psg.h"

#include "../sound_int.h"
#include "gym.h"
#include <zlib.h>
#include <bzlib.h>

extern int gym_update_needed;

namespace CODEC_GYM			{
	int cfg_samplerate = 44100;
	int cfg_ym2612_enable = 1;
	int cfg_ym2612_interp = 0;
	int cfg_chan1_enable = 1;
	int cfg_chan2_enable = 1;
	int cfg_chan3_enable = 1;
	int cfg_chan4_enable = 1;
	int cfg_chan5_enable = 1;
	int cfg_chan6_enable = 1;
	int cfg_dac_enable = 1;
	int cfg_psg_enable = 1;
	int cfg_psg_interp = 0;
	int cfg_psg_chan1_enable = 1;
	int cfg_psg_chan2_enable = 1;
	int cfg_psg_chan3_enable = 1;
	int cfg_psg_chan4_enable = 1;
	int cfg_num_loop = 1;
	int cfg_fade_length = 0;

	//#include "resource.h"

	/*static cfg_int cfg_samplerate("gym_samplerate", 48000);

	static cfg_int cfg_ym2612_enable("gym_ym2612_enable", 1);
	static cfg_int cfg_ym2612_interp("gym_ym2612_interp", 0);

	static cfg_int cfg_chan1_enable("gym_chan1_enable", 1);
	static cfg_int cfg_chan2_enable("gym_chan2_enable", 1);
	static cfg_int cfg_chan3_enable("gym_chan3_enable", 1);
	static cfg_int cfg_chan4_enable("gym_chan4_enable", 1);
	static cfg_int cfg_chan5_enable("gym_chan5_enable", 1);
	static cfg_int cfg_chan6_enable("gym_chan6_enable", 1);
	static cfg_int cfg_dac_enable("gym_dac_enable", 1);

	static cfg_int cfg_psg_enable("gym_psg_enable", 1);
	static cfg_int cfg_psg_interp("gym_psg_interp", 0);

	static cfg_int cfg_psg_chan1_enable("gym_psg_chan1_enable", 1);
	static cfg_int cfg_psg_chan2_enable("gym_psg_chan2_enable", 1);
	static cfg_int cfg_psg_chan3_enable("gym_psg_chan3_enable", 1);
	static cfg_int cfg_psg_chan4_enable("gym_psg_chan4_enable", 1);

	static cfg_int cfg_num_loop("gym_num_loop", 1);
	static cfg_int cfg_fade_length("gym_fade_length", 10);*/

	static class input_gym *g_decoder;
	//static critical_section g_decoder_sync;

	static int GetDwordLE(unsigned char *p)
	{
		int ret;
		ret  = ((int)(unsigned char)p[3]) << 0x18;
		ret |= ((int)(unsigned char)p[2]) << 0x10;
		ret |= ((int)(unsigned char)p[1]) << 0x08;
		ret |= ((int)(unsigned char)p[0]) << 0x00;
		return ret;
	}

	unsigned long decomp_dac(unsigned char *srcp, unsigned long srcl, unsigned char *desp, unsigned long desl)
	{
		if (strncmp((char *)srcp, "EZPK", 4)) return 0;
		unsigned long ul = GetDwordLE(srcp + 4);
		if (desl == 0) return ul & 0xfffffff;
		if ((ul & 0xf0000000) == 0)
		{
			unsigned int ui = desl;
			if (BZ2_bzBuffToBuffDecompress((char *)desp, &ui, (char *)(srcp + 8), srcl, 0, 0) < 0)
				return 0;
			return desl;
		}
		else
		{
			z_stream zs;
			memset(&zs, 0, sizeof(z_stream));
			zs.next_in = (Bytef *)(srcp + 8);
			zs.avail_in = srcl - 8;
			zs.next_out = (Bytef *)desp;
			zs.avail_out = desl;
			if ((inflateInit(&zs)) < 0) return 0;
			if ((inflate(&zs, Z_FINISH)) >= 0)
				ul = zs.total_out;
			else
				ul = 0;
			inflateEnd(&zs);
			return ul;
		}
	}

	void input_gym::set_frequency(int newFrequency)		{
		if (newFrequency != cfg_samplerate)		{
			cfg_samplerate = newFrequency;
			gym_update_needed = 1;
		}
	}

		int input_gym::load_ezpk_gym(void *gymFile)
		{
			unsigned int unezbuf_l = gym_tag -> compressed + 0x24;

			gym_start = new unsigned char[unezbuf_l];

			if (!gym_start)
			{
				gym_backup.set_size(0);

				gym_tag = 0;
				return 0;
			}

			unsigned char *unezbuf_v = new unsigned char[unezbuf_l];
			if (!unezbuf_v)
			{
				delete[] gym_start;
				gym_backup.set_size(0);

				gym_tag = 0;
				return 0;
			}

			int bzerr = BZ2_bzBuffToBuffDecompress((char *)unezbuf_v, &unezbuf_l, (char *)gymFile + sizeof(GYMTAG) + 8, gym_backup.get_size() - sizeof(GYMTAG) - 8, 0, 0);

			if(bzerr != BZ_OK)
			{
				delete[] unezbuf_v;
				delete[] gym_start;

				gym_backup.set_size(0);
				gym_start = 0;
				gym_tag = 0;
				return 0;
			}

			unsigned long undacbuf_l = GetDwordLE(&unezbuf_v[0x18]) + GetDwordLE(&unezbuf_v[0x1c]);
			if (undacbuf_l > 0) //>= 100)
			{
				unsigned char *undacbuf_v = new unsigned char[undacbuf_l];
				if (!undacbuf_v)
				{
					delete[] unezbuf_v;
					delete[] gym_start;

					gym_backup.set_size(0);
					gym_start = 0;
					gym_tag = 0;
					return 0;
				}

				unsigned long ret = decomp_dac(&unezbuf_v[GetDwordLE(&unezbuf_v[0x20])],GetDwordLE(&unezbuf_v[0x14]), &undacbuf_v[0], undacbuf_l);
				if (ret) memcpy(&unezbuf_v[GetDwordLE(&unezbuf_v[0x20])], &undacbuf_v[0], ret);

				delete[] undacbuf_v;
			}

			unsigned char *typep, *sngp, *regp, *datmp, *datsp, *dacmp, *dacsp;
			{
				unsigned char *p1;
				p1 = &unezbuf_v[0x24];
				typep = p1; p1 += GetDwordLE(&unezbuf_v[0x00]);
				regp  = p1; p1 += GetDwordLE(&unezbuf_v[0x04]);
				datmp = p1; p1 += GetDwordLE(&unezbuf_v[0x08]);
				datsp = p1; p1 += GetDwordLE(&unezbuf_v[0x0c]);
				sngp  = p1; p1 += GetDwordLE(&unezbuf_v[0x10]);
				dacmp = p1; p1 += GetDwordLE(&unezbuf_v[0x18]);
				dacsp = p1;
			}
			unsigned idx = 0;
			do {
				unsigned char uc;
				gym_start[idx++] = uc = *(typep++);
				switch (uc)
				{
					case 1:
						gym_start[idx++] = uc = *(regp++);
						if (uc == 0x2a && undacbuf_l > 0)
							gym_start[idx++] = *(dacmp++);
						else
							gym_start[idx++] = *(datmp++);
						break;
					case 2:
						gym_start[idx++] = uc = *(regp++);
						if (uc == 0x2a && undacbuf_l > 0)
							gym_start[idx++] = *(dacsp++);
						else
							gym_start[idx++] = *(datsp++);
						break;
					case 3:
						gym_start[idx++] = *(sngp++);
						break;
				}
			} while (idx < gym_tag -> compressed);

			delete[] unezbuf_v;

			gym_size = gym_tag -> compressed;

			return 1;
		}

		int input_gym::load_zlib_gym(void *gymFile)
		{
			unsigned long destsize = gym_tag -> compressed;

			gym_start = new unsigned char[destsize];
			if (!gym_start)
			{
				gym_backup.set_size(0);

				gym_tag = 0;
				return 0;
			}

			int zerr = uncompress(gym_start, &destsize, (unsigned char *)gymFile + sizeof(GYMTAG), gym_backup.get_size() - sizeof(GYMTAG));

			if(zerr != Z_OK)
			{
				delete[] gym_start;

				gym_backup.set_size(0);
				gym_start = 0;
				gym_tag = 0;
				return 0;
			}

			gym_size = destsize;

			return 1;
		}

		void input_gym::calc_gym_time_length(SONGINFO* info)
		{
			if (gym_start == 0 || gym_size == 0)
			{
				oslDebug("GYM not initialized properly for length calculation!");
			}

			int loop, num_zeros = 0;

			for(loop = 0; loop < gym_size; loop++)
			{
				switch(gym_start[loop])
				{
					case(0x00):
						num_zeros++;
						continue;
					case(0x01):
						loop += 2;
						continue;
					case(0x02):
						loop += 2;
						continue;
					case(0x03):
						loop += 1;
						continue;
				}
			}

			song_length = num_zeros / fnum(60.0) * fnum(1000.0);
			if (gym_loop)
			{
				loop_length = (song_length - gym_loop / fnum(60.0) * fnum(1000.0));
				gym_length = song_length + cfg_num_loop * loop_length;
				gym_length_with_fade = gym_length + fnum(1000.0) * cfg_fade_length;
			}
			else
			{
				loop_length = fnum(0.0);
				gym_length = gym_length_with_fade = song_length;
			}


			info->set_length(gym_length_with_fade / fnum(1000.0));
		}

		int input_gym::open_file(CFILE *r, SONGINFO *info, unsigned int flags)
		{
			__int64 size64 = r->get_length();
			if (size64 < 0) return 0;
			unsigned int size = (unsigned int)size64;

			if (!gym_backup.new_block(size))
				return 0;

			void *gymFile = gym_backup.get_ptr();

			if (r->read(gymFile, size) != size)
			{
				r->seek(0);
				return 0;
			}

			gym_tag = (GYMTAG *)gym_backup.get_ptr();

			if (memcmp(gym_tag->gym_id, "GYMX", 4) == 0)
			{
				// strings might not be null-terminated...
				char tempStr[257];

				memcpy(tempStr, gym_tag->song_title, 32);
				tempStr[32] = '\0';
				if (strcmp(tempStr, "") != 0)
				{
					info->meta_add_ansi("TITLE", tempStr);
				}

				memcpy(tempStr, gym_tag->game_title, 32);
				tempStr[32] = '\0';
				if (strcmp(tempStr, "") != 0)
				{
					info->meta_add_ansi("ALBUM", tempStr);
				}

				memcpy(tempStr, gym_tag->game_publisher, 32);
				tempStr[32] = '\0';
				if (strcmp(tempStr, "") != 0)
				{
					info->meta_add_ansi("PUBLISHER", tempStr);
				}

				memcpy(tempStr, gym_tag->dumper_emu, 32);
				tempStr[32] = '\0';
				if (strcmp(tempStr, "") != 0)
				{
					info->meta_add_ansi("EMULATOR", tempStr);
				}

				memcpy(tempStr, gym_tag->dumper_person, 32);
				tempStr[32] = '\0';
				if (strcmp(tempStr, "") != 0)
				{
					info->meta_add_ansi("DUMPER", tempStr);
				}

				memcpy(tempStr, gym_tag->comments, 256);
				tempStr[256] = '\0';
				if (strcmp(tempStr, "") != 0)
				{
					info->meta_add_ansi("COMMENT", tempStr);
				}

				gym_loop = gym_tag->looped;

				if (gym_loop)
				{
					sprintf(tempStr, "%.2f seconds", gym_loop / 60.0);
					info->meta_add_ansi("LOOPSAT", tempStr);
				}

				if (gym_tag->compressed)
				{
					gym_compressed = true;
					if (memcmp((char *)((int)gym_tag + sizeof(GYMTAG)), "EZPK", 4) == 0)
					{
						info->meta_add_ansi("COMPRESSION", "EZPK");

						if (!gym_loaded)
						{
							if (!load_ezpk_gym(gymFile)) return 0;
							calc_gym_time_length(info);
							gym_loaded = true;
						}
					}
					else
					{
						info->meta_add_ansi("COMPRESSION", "ZLIB");

						if (!gym_loaded)
						{
							if (!load_zlib_gym(gymFile)) return 0;

							calc_gym_time_length(info);
							gym_loaded = true;
						}
					}
				}
				else
				{
					gym_compressed = false;
					info->meta_add_ansi("COMPRESSION", "none");

					gym_start = (unsigned char *)gymFile + sizeof(GYMTAG);
					gym_size = gym_backup.get_size() - sizeof(GYMTAG);

					if (!gym_loaded)
					{
						calc_gym_time_length(info);
						gym_loaded = true;
					}
				}

			}
			else
			{
				gym_tag = 0;
				gym_loop = 0;

				gym_compressed = false;
				info->meta_add_ansi("COMPRESSION", "none");

				gym_start = (unsigned char *)gymFile;
				gym_size = gym_backup.get_size();

				if (!gym_loaded)
				{
					calc_gym_time_length(info);
					gym_loaded = true;
				}
			}

			info->info_set("codec", "GYM");
			info->info_set("extrainfo", "GYM");

			return 1;
		}

		void input_gym::construct()		{
			gym_pos = 0;
			gym_start = 0;
			gym_compressed = false;
			gym_loaded = false;
			gym_size = 0;
			gym_loop = 0;
			gym_tag = 0;
			gym_length = fnum(0.0);
			gym_length_with_fade = fnum(0.0);
			gym_pos_ms = fnum(0.0);
		}

		void input_gym::destruct()		{
			if (gym_start != 0 && gym_compressed == true)
			{
				delete[] gym_start;
				gym_start = 0;
			}

			gym_compressed = false;
			gym_loaded = false;
			gym_pos = 0;

	//		g_decoder_sync.enter();
			if (g_decoder==this) g_decoder=0;
	//		g_decoder_sync.leave();
		}

		input_gym::input_gym()
		{
			construct();
		}

		input_gym::~input_gym()
		{
			destruct();
		}

		void input_gym::reset()		{
			destruct();
			construct();
		}
		
		char input_gym::open(CFILE * r, SONGINFO * info, unsigned int flags)	//multiinstance safety ?
		{
			if (!open_file(r,info,flags)) return 0;
//			if (!(flags & OPEN_FLAG_DECODE)) return 1;

			char failed = false;
	//		g_decoder_sync.enter();
			if (g_decoder == 0) g_decoder = this;
			else if (g_decoder!=this) failed = true;
	//		g_decoder_sync.leave();
			if (failed)
			{
				oslDebug("GYM decoder does not support multiple instances; please stop other decoding processes and try again");
				return 0;
			}

			void *gymFile = gym_backup.get_ptr();

			if (!gym_loaded)
			{
				if (gym_tag)
				{
					if (gym_tag -> compressed)
					{
						if (memcmp((char *)((int)gymFile + sizeof(GYMTAG)), "EZPK", 4) == 0)
						{
							// EZPK-compressed
							if (!load_ezpk_gym(gymFile)) return 0;
						}
						else
						{
							// standard zlib-compressed
							if (!load_zlib_gym(gymFile)) return 0;
						}
					}
					else
					{
						// not compressed
						gym_start = (unsigned char *)gymFile + sizeof(GYMTAG);
						gym_size = gym_backup.get_size() - sizeof(GYMTAG);
					}
				}
				else
				{
					// no tag, assume not compressed
					gym_start = (unsigned char *)gymFile;
					gym_size = gym_backup.get_size();
				}

				gym_loaded = true;
			}
				
			gym_pos = gym_start;

			YM2612_Enable = cfg_ym2612_enable;
			YM2612_Improv = cfg_ym2612_interp;

			Chan_Enable[0] = cfg_chan1_enable;
			Chan_Enable[1] = cfg_chan2_enable;
			Chan_Enable[2] = cfg_chan3_enable;
			Chan_Enable[3] = cfg_chan4_enable;
			Chan_Enable[4] = cfg_chan5_enable;
			Chan_Enable[5] = cfg_chan6_enable;
			DAC_Enable = cfg_dac_enable;

			PSG_Enable = cfg_psg_enable;
			PSG_Improv = cfg_psg_interp;

			PSG_Chan_Enable[0] = cfg_psg_chan1_enable;
			PSG_Chan_Enable[1] = cfg_psg_chan2_enable;
			PSG_Chan_Enable[2] = cfg_psg_chan3_enable;
			PSG_Chan_Enable[3] = cfg_psg_chan4_enable;

			playingSampleRate = cfg_samplerate;

			Start_Play_GYM(playingSampleRate);

//			sample_buffer.check_size(Seg_Lenght << 2);
			sample_buffer.new_block(Seg_Lenght << 2);

			return 1;
		}

		int input_gym::get_samples_pcm(void ** buffer,int * size,int * srate,int * bps,int * nch)
		{
			*srate = playingSampleRate;
			*bps = 16;
			*nch = 2;

//			oslDebug("%x %x %x %x %x", sample_buffer.get_ptr(), gym_start, gym_pos, gym_size, gym_loop);
			unsigned char *new_gym_pos = Play_GYM(sample_buffer.get_ptr(), gym_start, gym_pos, gym_size, gym_loop);
//			oslDebug("%x %x %x %x %x", sample_buffer.get_ptr(), gym_start, gym_pos, gym_size, gym_loop);

			if (new_gym_pos == 0 || (gym_length_with_fade > 0 && gym_pos_ms > gym_length_with_fade))
			{
				return 0;
			}

			gym_pos_ms += fnum(1000.0) / fnum(60.0);
			gym_pos = new_gym_pos;

			if (gym_loop && gym_length > 0 && gym_pos_ms > gym_length)
			{
				// this isn't as precise as it could be as it's not taking position
				// within this update into account but it's unlikely one will notice the
				// ramp down in volume in 60th-second-long steps
				short *foo = (short*)sample_buffer.get_ptr();
				int fade_pos = (int)(gym_length_with_fade - gym_pos_ms);;
				
				for (int n = 0; n < Seg_Lenght << 2; n++)
				{
					*foo = *foo;
					foo++;
//					*foo++ = MulDiv(*foo, fade_pos, cfg_fade_length * 1000);
				}
			}

			*buffer = sample_buffer.get_ptr();
			*size =	Seg_Lenght << 2;

			return 1;

		}

		char input_gym::seek (double seconds)
		{
			double exact_new_pos;

			if (seconds < fnum(0.0))
			{
				gym_pos_ms = exact_new_pos = seconds = fnum(0.0);
			}
			else if (seconds * fnum(1000.0) < song_length)
			{
				exact_new_pos = floor(fnum(60.0) * seconds);
				gym_pos_ms = exact_new_pos * fnum(1000.0) / fnum(60.0);
			}
			else
			{
				gym_pos_ms = floor(fnum(60.0) * seconds) * fnum(1000.0) / fnum(60.0);

				seconds -= song_length / fnum(1000.0);
				while (seconds * fnum(1000.0) > loop_length)
				{
					seconds -= loop_length / fnum(1000.0);
				}
				seconds += gym_loop / fnum(60.0);

				exact_new_pos = floor(fnum(60.0) * seconds);
			}

			gym_pos = jump_gym_time_pos(gym_start, gym_size, (int)exact_new_pos);

			return true;
		}

/*		virtual set_info_t set_info(CFILE *r, const SONGINFO * info)
		{
			tag_remover::g_run(r);
			return tag_writer::g_run(r,info,"gym") ? SET_INFO_SUCCESS : SET_INFO_FAILURE;
		}*/

	/*static BOOL CALLBACK ConfigProc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
	{
		switch(msg)
		{
		case WM_INITDIALOG:
			{
				int index;
				int sampRate;
				char sampleRate[6];

				const int sampleRates[] = {8000, 11025, 16000, 22050, 24000,
											32000, 44100, 48000, 64000, 88200,
											96000};
				const int numSampleRates = 11;

				uSendDlgItemMessage(wnd, IDC_SAMPLERATE, CB_RESETCONTENT, 0, 0);

				for (int i = 0; i < numSampleRates; i++)
				{
					itoa(sampleRates[i], sampleRate, 10);
					index = uSendDlgItemMessageText(wnd, IDC_SAMPLERATE, CB_INSERTSTRING, -1, (const char *) sampleRate);
					uSendDlgItemMessage(wnd, IDC_SAMPLERATE, CB_SETITEMDATA, index, sampleRates[i]);
				}
				
				sampRate = cfg_samplerate;
				sprintf(sampleRate, "%d", sampRate);
				index = uSendDlgItemMessageText(wnd, IDC_SAMPLERATE, CB_FINDSTRINGEXACT, -1, (const char *) sampleRate);
				if (index == CB_ERR)
				{
					cfg_samplerate = 48000;
					index = uSendDlgItemMessageText(wnd, IDC_SAMPLERATE, CB_FINDSTRINGEXACT, -1, (const char *) "48000");
				}

				uSendDlgItemMessage(wnd, IDC_SAMPLERATE, CB_SETCURSEL, index, -1);

				uSendDlgItemMessage(wnd, IDC_YM2612_ENABLE, BM_SETCHECK, cfg_ym2612_enable, 0);
				uSendDlgItemMessage(wnd, IDC_YM2612_INTERP, BM_SETCHECK, cfg_ym2612_interp, 0);

				uSendDlgItemMessage(wnd, IDC_CHAN1_ENABLE, BM_SETCHECK, cfg_chan1_enable, 0);
				uSendDlgItemMessage(wnd, IDC_CHAN2_ENABLE, BM_SETCHECK, cfg_chan2_enable, 0);
				uSendDlgItemMessage(wnd, IDC_CHAN3_ENABLE, BM_SETCHECK, cfg_chan3_enable, 0);
				uSendDlgItemMessage(wnd, IDC_CHAN4_ENABLE, BM_SETCHECK, cfg_chan4_enable, 0);
				uSendDlgItemMessage(wnd, IDC_CHAN5_ENABLE, BM_SETCHECK, cfg_chan5_enable, 0);
				uSendDlgItemMessage(wnd, IDC_CHAN6_ENABLE, BM_SETCHECK, cfg_chan6_enable, 0);
				uSendDlgItemMessage(wnd, IDC_DAC_ENABLE, BM_SETCHECK, cfg_dac_enable, 0);

				uSendDlgItemMessage(wnd, IDC_PSG_ENABLE, BM_SETCHECK, cfg_psg_enable, 0);
				uSendDlgItemMessage(wnd, IDC_PSG_INTERP, BM_SETCHECK, cfg_psg_interp, 0);

				uSendDlgItemMessage(wnd, IDC_PSG_CHAN1_ENABLE, BM_SETCHECK, cfg_psg_chan1_enable, 0);
				uSendDlgItemMessage(wnd, IDC_PSG_CHAN2_ENABLE, BM_SETCHECK, cfg_psg_chan2_enable, 0);
				uSendDlgItemMessage(wnd, IDC_PSG_CHAN3_ENABLE, BM_SETCHECK, cfg_psg_chan3_enable, 0);
				uSendDlgItemMessage(wnd, IDC_PSG_CHAN4_ENABLE, BM_SETCHECK, cfg_psg_chan4_enable, 0);

				char tempStr[16];
				int temp = cfg_num_loop;
				sprintf(tempStr, "%d", temp);
				uSetDlgItemText(wnd, IDC_NUMLOOP, tempStr);

				temp = cfg_fade_length;
				sprintf(tempStr, "%d", temp);
				uSetDlgItemText(wnd, IDC_FADELENGTH, tempStr);
			}
			return 1;
		case WM_COMMAND:
			switch(LOWORD(wp))
			{
			case IDC_SAMPLERATE:
				if (HIWORD(wp) == CBN_SELCHANGE)
				{
					int index;
					int sampRate;

					index = uSendMessage((HWND)lp, CB_GETCURSEL, 0, 0);
					sampRate = uSendMessage((HWND)lp, CB_GETITEMDATA, index, 0);
					if (sampRate != CB_ERR)
					{
						cfg_samplerate = sampRate;
					}

				}
				break;
			case IDC_YM2612_ENABLE:
				cfg_ym2612_enable = uSendMessage((HWND)lp,BM_GETCHECK,0,0);
				YM2612_Enable = cfg_ym2612_enable;
				break;

			case IDC_YM2612_INTERP:
				cfg_ym2612_interp = uSendMessage((HWND)lp,BM_GETCHECK,0,0);
				YM2612_Improv = cfg_ym2612_interp;
				break;

			case IDC_CHAN1_ENABLE:
				cfg_chan1_enable = uSendMessage((HWND)lp,BM_GETCHECK,0,0);
				Chan_Enable[0] = cfg_chan1_enable;
				break;

			case IDC_CHAN2_ENABLE:
				cfg_chan2_enable = uSendMessage((HWND)lp,BM_GETCHECK,0,0);
				Chan_Enable[1] = cfg_chan2_enable;
				break;

			case IDC_CHAN3_ENABLE:
				cfg_chan3_enable = uSendMessage((HWND)lp,BM_GETCHECK,0,0);
				Chan_Enable[2] = cfg_chan3_enable;
				break;

			case IDC_CHAN4_ENABLE:
				cfg_chan4_enable = uSendMessage((HWND)lp,BM_GETCHECK,0,0);
				Chan_Enable[3] = cfg_chan4_enable;
				break;

			case IDC_CHAN5_ENABLE:
				cfg_chan5_enable = uSendMessage((HWND)lp,BM_GETCHECK,0,0);
				Chan_Enable[4] = cfg_chan5_enable;
				break;

			case IDC_CHAN6_ENABLE:
				cfg_chan6_enable = uSendMessage((HWND)lp,BM_GETCHECK,0,0);
				Chan_Enable[5] = cfg_chan6_enable;
				break;

			case IDC_DAC_ENABLE:
				cfg_dac_enable = uSendMessage((HWND)lp,BM_GETCHECK,0,0);
				DAC_Enable = cfg_dac_enable;
				break;

			case IDC_PSG_ENABLE:
				cfg_psg_enable = uSendMessage((HWND)lp,BM_GETCHECK,0,0);
				PSG_Enable = cfg_psg_enable;
				break;

			case IDC_PSG_INTERP:
				cfg_psg_interp = uSendMessage((HWND)lp,BM_GETCHECK,0,0);
				PSG_Improv = cfg_psg_interp;
				break;

			case IDC_PSG_CHAN1_ENABLE:
				cfg_psg_chan1_enable = uSendMessage((HWND)lp,BM_GETCHECK,0,0);
				PSG_Chan_Enable[0] = cfg_psg_chan1_enable;
				break;

			case IDC_PSG_CHAN2_ENABLE:
				cfg_psg_chan2_enable = uSendMessage((HWND)lp,BM_GETCHECK,0,0);
				PSG_Chan_Enable[1] = cfg_psg_chan2_enable;
				break;

			case IDC_PSG_CHAN3_ENABLE:
				cfg_psg_chan3_enable = uSendMessage((HWND)lp,BM_GETCHECK,0,0);
				PSG_Chan_Enable[2] = cfg_psg_chan3_enable;
				break;

			case IDC_PSG_CHAN4_ENABLE:
				cfg_psg_chan4_enable = uSendMessage((HWND)lp,BM_GETCHECK,0,0);
				PSG_Chan_Enable[3] = cfg_psg_chan4_enable;
				break;

			case IDC_FADELENGTH:
				if (HIWORD(wp) == EN_CHANGE)
				{
					int test = atoi(string_utf8_from_window((HWND)lp));
					if (test > 0)
					{
						cfg_fade_length = test;
					}
				}
				break;

			case IDC_NUMLOOP:
				if (HIWORD(wp) == EN_CHANGE)
				{
					int test = atoi(string_utf8_from_window((HWND)lp));
					if (test > 0)
					{
						cfg_num_loop = test;
					}
				}
				break;
			}
		}
		return 0;
	}

	class config_gym : public config
	{
	public:
		virtual HWND create(HWND parent)
		{
			return uCreateDialog(IDD_CONFIG,parent,ConfigProc);
		}

		virtual const char * get_name() {return "GYM Decoder";}
		virtual const char * get_parent_name() {return "Input";}
	};*/

//	static service_factory_t<input,input_gym> foo_gym;
//	static service_factory_single_t<config,config_gym> foo_gym_config;

//	DECLARE_COMPONENT_VERSION("GYM Decoder","1.5","Based on Gens v2.12a and information from YMAMP and kpigym");
}
