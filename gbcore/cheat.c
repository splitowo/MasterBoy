/*--------------------------------------------------
   TGB Dual - Gameboy Emulator -
   Copyright (C) 2001  Hii

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "gb.h"
#include <ctype.h>

#ifdef CHEAT_SUPPORT

#define MAX_CHEATS 128

byte cheat_map[0x10000];
cheat_dat cheats[MAX_CHEATS];
int nCheats;
int cheat_enable = 0;

void cheat_clear()
{
	nCheats = 0;
	cheat_create_cheat_map();
}

void cheat_init()
{
	cheat_clear();
}

void cheat_decreate_cheat_map()
{
	int i;

	for (i=0; i<nCheats; i++){
		cheat_dat *tmp=&cheats[i];

		if (!tmp->enable)
			continue;

		switch(tmp->code){
		case 0x01:
			cpu_write_direct(tmp->adr, tmp->dat_old);
			break;
		case 0x90:
		case 0x91:
		case 0x92:
		case 0x93:
		case 0x94:
		case 0x95:
		case 0x96:
		case 0x97:
			if ((tmp->adr>=0xD000)&&(tmp->adr<0xE000)){
				int ram_adr = (tmp->code-0x90)*0x1000 + tmp->adr - 0xD000;
				ram[ram_adr] = tmp->dat_old;
			}
			else{
				cpu_write_direct(tmp->adr, tmp->dat_old);
			}
			break;
		case 0xA0:
		case 0xA1:
			if(tmp->adr < 0x4000){
				get_rom()[tmp->adr] = tmp->dat_old;
			}
			else{
				//restore previous values for each bank unconditionally
				int num_rom_switchable_banks = 2 * (1 << rom_get_info()->rom_size) - 1;
				int j;
				for(j = 0; j < num_rom_switchable_banks; j++){
					get_rom()[0x4000 * j + tmp->adr] = tmp->dat_old_rom_banks[j];
				}
				free(tmp->dat_old_rom_banks);
			}
			break;
		}
	}
}

void cheat_create_cheat_map()
{
	int i;

	memset(cheat_map,0,sizeof(cheat_map));

	for (i=0; i<nCheats; i++){
		cheat_dat *tmp=&cheats[i];

		if (!tmp->enable)
			continue;

		cheat_map[tmp->adr] = 1 + i;

		switch(tmp->code){
		case 0x00:
			cpu_write_direct(tmp->adr, tmp->dat);
			tmp->enable = false;
			cheat_map[tmp->adr] = 0;
			break;
		case 0x01:
			tmp->dat_old = cpu_read(tmp->adr);
			cpu_write_direct(tmp->adr, tmp->dat);
			break;
		case 0x90:
		case 0x91:
		case 0x92:
		case 0x93:
		case 0x94:
		case 0x95:
		case 0x96:
		case 0x97:
			if ((tmp->adr>=0xD000)&&(tmp->adr<0xE000)){
				int ram_adr = (tmp->code-0x90)*0x1000 + tmp->adr - 0xD000;
				tmp->dat_old = ram[ram_adr];
				ram[ram_adr] = tmp->dat;
			}
			else{
				tmp->dat_old = cpu_read(tmp->adr);
				cpu_write_direct(tmp->adr, tmp->dat);
			}
			break;
		case 0xA0:
			// no check value: write value to all ROM banks
			if(tmp->adr < 0x4000){
				tmp->dat_old = get_rom()[tmp->adr];
				get_rom()[tmp->adr] = tmp->dat;
			}
			else{
				int num_rom_switchable_banks = 2 * (1 << rom_get_info()->rom_size) - 1;
				tmp->dat_old_rom_banks = malloc(sizeof(byte) * num_rom_switchable_banks);
				int j;
				for(j = 0; j < num_rom_switchable_banks; j++){
					tmp->dat_old_rom_banks[j] = get_rom()[0x4000 * j + tmp->adr];
					get_rom()[0x4000 * j + tmp->adr] = tmp->dat;
				}
			}
			break;
		case 0xA1:
			// check value: write value to applicable ROM banks only
			if(tmp->adr < 0x4000){
				tmp->dat_old = get_rom()[tmp->adr];
				get_rom()[tmp->adr] = tmp->dat;
			}
			else{
				int num_rom_switchable_banks = 2 * (1 << rom_get_info()->rom_size) - 1;
				tmp->dat_old_rom_banks = malloc(sizeof(byte) * num_rom_switchable_banks);
				int j;
				for(j = 0; j < num_rom_switchable_banks; j++){
					tmp->dat_old_rom_banks[j] = get_rom()[0x4000 * j + tmp->adr];
					if(tmp->dat_old_rom_banks[j] == tmp->check_dat){
						get_rom()[0x4000 * j + tmp->adr] = tmp->dat; // current state: cheats appear to be working. to address: possible visual glitches present, either from writing to too many ROM banks or from other reasons
					}
				}
			}
			break;
		}
	}
}

byte cheat_write(word adr, byte dat)
{
	cheat_dat *tmp=&cheats[cheat_map[adr] - 1];

	switch(tmp->code){
	case 0x01:
		tmp->dat_old = dat;
		return tmp->dat;
	case 0x90:
	case 0x91:
	case 0x92:
	case 0x93:
	case 0x94:
	case 0x95:
	case 0x96:
	case 0x97:
		if ((adr>=0xD000)&&(adr<0xE000)){
			if(((cpu_get_ram_bank()-cpu_get_ram())/0x1000)==(tmp->code-0x90)){
				tmp->dat_old = dat;
				return tmp->dat;
			}
		}
		else{
			tmp->dat_old = dat;
			return tmp->dat;
		}
	}

	return dat;
}

int hex2n(char c)
{
	c = toupper(c);
	return (isalpha(c)?(c-'A'+10):(c-'0'));
}

int cheat_load(FILE *file)
{
	cheat_dat tmp_dat;
	char buf[256];
	int i;
	char first=true;

	cheat_decreate_cheat_map();
	cheat_clear();

	while(!feof(file)){
		if (fgets(buf,256,file) && buf[0]!='\n' && buf[0]!='\r'){
			if (first){
				for (i=0;i<256;i++){
					if (buf[i]=='\n' || buf[i]=='\r'){
						buf[i]='\0';
						break;
					}
				}
				strcpy(tmp_dat.name,buf);
				first=false;
			}
			else{
				for (i=0;i<256;i++){
					if (buf[i]=='\n' || buf[i]=='\r'){
						buf[i]='\0';
						break;
					}
				}
				if (i == 8){ // Gameshark code
					tmp_dat.code = hex2n(buf[0])<< 4 | hex2n(buf[1]);
					tmp_dat.dat  = hex2n(buf[2])<< 4 | hex2n(buf[3]);
					tmp_dat.adr  = hex2n(buf[6])<<12 | hex2n(buf[7])<<8 | hex2n(buf[4])<<4 | hex2n(buf[5]);
					tmp_dat.enable = true;
					cheats[nCheats++] = tmp_dat;

					first = true;
				}
				else if (i == 7){ // Gamegenie code 6-char
					tmp_dat.code = 0xA0;
					tmp_dat.dat = hex2n(buf[0])<< 4 | hex2n(buf[1]);
					tmp_dat.adr = (hex2n(buf[6]) ^ 0xF) << 12 | hex2n(buf[2])<< 8 | hex2n(buf[4]) << 4 | hex2n(buf[5]);
					tmp_dat.enable = true;
					cheats[nCheats++] = tmp_dat;

					first = true;
				}
				else if (i == 11){ // Gamegenie code 9-char
					tmp_dat.code = 0xA1;
					tmp_dat.dat = hex2n(buf[0])<< 4 | hex2n(buf[1]);
					tmp_dat.adr = (hex2n(buf[6]) ^ 0xF) << 12 | hex2n(buf[2])<< 8 | hex2n(buf[4]) << 4 | hex2n(buf[5]);
					uint32_t check = hex2n(buf[8]) << 4 | hex2n(buf[10]);
					check = check >> 2 | (check) << 6;
					check &= 0xFF;
					check ^= 0xBA;
					tmp_dat.check_dat = check;
					tmp_dat.enable = true;
					cheats[nCheats++] = tmp_dat;

					first = true;
				}
				else {
					cheat_clear();
					return 0;
				}
			}
		}
		if (nCheats >= MAX_CHEATS)
			break;
	}

	cheat_create_cheat_map();

	return 1;
}
#endif

