/**************************************************************************************
 *                                                                                    *
 * This application contains code from OpenDivX and is released as a "Larger Work"    *
 * under that license. Consistant with that license, this application is released     *
 * under the GNU General Public License.                                              *
 *                                                                                    *
 * The OpenDivX license can be found at: http://www.projectmayo.com/opendivx/docs.php *
 * The GPL can be found at: http://www.gnu.org/copyleft/gpl.html                      *
 *                                                                                    *
 * Authors: Damien Chavarria <roy204 at projectmayo.com>                              *
 *                                                                                    *
 **************************************************************************************/

#include <stdlib.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "mpg123.h"

struct parameter param = { 1 , 1 , 0 , 0 };

int tabsel_123[2][3][16] = {
   { {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,},
     {0,32,48,56, 64, 80, 96,112,128,160,192,224,256,320,384,},
     {0,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320,} },

   { {0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,} }
};

long freqs[9] = { 44100, 48000, 32000,
                  22050, 24000, 16000 ,
                  11025 , 12000 , 8000 };


int bitindex;
uint8_t *wordpointer;
uint8_t *pcm_sample;
int pcm_point = 0;


#define HDRCMPMASK 0xfffffd00


/**
 * decode a header and write the information
 * into the frame structure
 **/

int head_check(unsigned long head)
{
  if(head == 0xFF)
	  return FALSE;
  if ((head & 0xffe00000) != 0xffe00000)
    return FALSE;
  if (!((head >> 17) & 3))
    return FALSE;
  if (((head >> 12) & 0xf) == 0xf)
    return FALSE;
  if (!((head >> 12) & 0xf))
    return FALSE;
  if (((head >> 10) & 0x3) == 0x3)
    return FALSE;
  if (((head >> 19) & 1) == 1 && ((head >> 17) & 3) == 3 && ((head >> 16) & 1) == 1)
    return FALSE;
  if ((head & 0xffff0000) == 0xfffe0000)
    return FALSE;
  
  return TRUE;
}


int decode_header(struct frame *fr,unsigned long newhead)
{
	if( newhead & (1<<20) ) {
	  fr->lsf = (newhead & (1<<19)) ? 0x0 : 0x1;
	  fr->mpeg25 = 0;
	} else {
	  fr->lsf = 1;
	  fr->mpeg25 = 1;
	}
	
	fr->lay = 4-((newhead>>17)&3);
	
	if( ((newhead>>10)&0x3) == 0x3) {
	  
	  return -1;
	}

	if(fr->mpeg25) 
	  fr->sampling_frequency = 6 + ((newhead>>10)&0x3);
	else
	  fr->sampling_frequency = ((newhead>>10)&0x3) + (fr->lsf*3);

	fr->error_protection = ((newhead>>16)&0x1)^0x1;

	if(fr->mpeg25) /* allow Bitrate change for 2.5 ... */
		fr->bitrate_index = ((newhead>>12)&0xf);

	fr->bitrate_index = ((newhead>>12)&0xf);
	fr->padding   = ((newhead>>9)&0x1);
	fr->extension = ((newhead>>8)&0x1);
	fr->mode      = ((newhead>>6)&0x3);
	fr->mode_ext  = ((newhead>>4)&0x3);
	fr->copyright = ((newhead>>3)&0x1);
	fr->original  = ((newhead>>2)&0x1);
	fr->emphasis  = newhead & 0x3;

	fr->stereo    = (fr->mode == MPG_MD_MONO) ? 1 : 2;

	if(!fr->bitrate_index) {
	  return -1;
	}

	switch(fr->lay) {
	case 1:
#if 0
		fr->jsbound = (fr->mode == MPG_MD_JOINT_STEREO) ? 
			(fr->mode_ext<<2)+4 : 32;
#endif
		fr->framesize  = (long) tabsel_123[fr->lsf][0][fr->bitrate_index] * 12000;
		fr->framesize /= freqs[fr->sampling_frequency];
		fr->framesize  = ((fr->framesize+fr->padding)<<2)-4;
		break;
	case 2:
#if 0
		fr->jsbound = (fr->mode == MPG_MD_JOINT_STEREO) ?
			(fr->mode_ext<<2)+4 : fr->II_sblimit;
#endif
		fr->framesize = (long) tabsel_123[fr->lsf][1][fr->bitrate_index] * 144000;
		fr->framesize /= freqs[fr->sampling_frequency];
		fr->framesize += fr->padding - 4;
		break;
	case 3:
#if 0
		fr->do_layer = do_layer3;
		if(fr->lsf)
			ssize = (fr->stereo == 1) ? 9 : 17;
		else
			ssize = (fr->stereo == 1) ? 17 : 32;
#endif

		fr->framesize  = (long) tabsel_123[fr->lsf][2][fr->bitrate_index] * 144000;
		fr->framesize /= freqs[fr->sampling_frequency]<<(fr->lsf);
		fr->framesize = fr->framesize + fr->padding - 4;
		break; 

	default:
	  return -1;
	}

	return 0;
}


/**
 *
 **/

uint32_t getbits (int number_of_bits)
{
	uint32_t rval;

	if(!number_of_bits)
		return 0;

	rval = wordpointer[0];
	rval <<= 8;
	rval |= wordpointer[1];
	rval <<= 8;
	rval |= wordpointer[2];
	rval <<= bitindex;
	rval &= 0xffffff;
	bitindex += number_of_bits;

	rval >>= (24-number_of_bits);

	wordpointer += (bitindex>>3);
	bitindex &= 7;

	return rval;
}


/**
 *
 **/

uint16_t getbits_fast (int number_of_bits)
{
	uint16_t rval;

	rval = wordpointer[0];
	rval <<= 8;	
	rval |= wordpointer[1];
	rval <<= bitindex;
	rval &= 0xffff;
	bitindex += number_of_bits;

	rval >>= (16-number_of_bits);

	wordpointer += (bitindex>>3);
	bitindex &= 7;

	return rval;
}


/**
 *
 **/

uint8_t get1bit(void)
{
	uint8_t rval;
	rval = *wordpointer << bitindex;

	bitindex++;
	wordpointer += (bitindex>>3);
	bitindex &= 7;

	return rval>>7;
}



