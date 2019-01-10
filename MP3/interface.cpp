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
#include <stdio.h>

#include <windows.h>

#include "mpg123.h"
#include "mpglib.h"

struct mpstr *gmp;

extern int head_check(unsigned long head);

/**
 *
 **/

BOOL InitMP3 (struct mpstr *mp) 
{
	static int init = 0;

	memset (mp, 0, sizeof (struct mpstr));

	mp->framesize = 0;
	mp->fsizeold = -1;
	mp->bsize = 0;
	mp->head = mp->tail = NULL;
	mp->fr.single = -1;
	mp->bsnum = 0;
	mp->synth_bo = 1;
	mp->tail = NULL;

	if (!init) {
		init = 1;
		make_decode_tables (32767);
		init_layer2 ();
		init_layer3 (SBLIMIT);
	}

	return 0;
}


/**
 *
 **/

void ExitMP3 (struct mpstr *mp)
{
	struct buf *b,*bn;

	b = mp->tail;
	
	while(b != NULL) {
		
		if(b->pnt != NULL) {
			
			free(b->pnt);
		}
		
		bn = b->next;
		free(b);
		
		b = bn;
	}

	mp->tail = NULL;
}


/**
 *
 **/

static struct buf *addbuf (struct mpstr *mp,char *buf,int size)
{
	struct buf *nbuf;

	if (!(nbuf = (struct buf *) new char[sizeof(struct buf)])) {
	  return NULL;
	}

	if (!(nbuf->pnt = (unsigned char *) new char[size])) {
		free (nbuf);
		return NULL;
	}
	nbuf->size = size;
	memcpy (nbuf->pnt,buf,size);
	nbuf->next = NULL;
	nbuf->prev = mp->head;
	nbuf->pos = 0;

	if(!mp->tail)
		mp->tail = nbuf;
	else
		mp->head->next = nbuf;

	mp->head = nbuf;
	mp->bsize += size;

	return nbuf;
}


/**
 *
 **/

static void remove_buf(struct mpstr *mp)
{
	struct buf *buf = mp->tail;
  
	if ((mp->tail = buf->next))
		mp->tail->prev = NULL;
	else 
		mp->tail = mp->head = NULL;
  
	free (buf->pnt);
	free (buf);
}


/**
 *
 **/

static int _read_buf_byte (struct mpstr *mp)
{
	unsigned int b;

	int pos;

	if(mp == NULL || mp->tail == NULL) {
	  return -1;
	}

	pos = mp->tail->pos;
	while (pos >= mp->tail->size) {
		remove_buf (mp);
		
		if(mp == NULL || mp->tail == NULL) {
		  return -1;
		}
		
		pos = mp->tail->pos;
		if (!mp->tail) {
		  return -1;
		}
	}

	b = mp->tail->pnt[pos];
	mp->bsize--;
	mp->tail->pos++;
	

	return b;
}


/**
 *
 **/


static void read_head (struct mpstr *mp)
{
  uint32_t head;

  head = _read_buf_byte (mp) & 0xFF;

  mp->header <<= 8;
  mp->header |= head;
}


/**
 *
 **/

int decodeMP3(struct mpstr *mp,char *in,int isize,char *out,
		int osize,int *done)
{
	int len;

	gmp = mp;

	//MessageBox(NULL, "decodeMP3", "", MB_OK);

	if(osize < 4608) {
	  return MP3_ERR;
	}

	if(in  && !addbuf(mp, in, isize)) {
	  return MP3_ERR;
	}

	//MessageBox(NULL, "addBuffer", "", MB_OK);

	/* First decode header */
	if (!mp->framesize) {
	  
	  if(mp->bsize < 4)
	    return MP3_NEED_MORE;
	  
	  read_head(mp);
	  
	  while(!head_check(mp->header)) {
	    read_head(mp);
	  }

	  //MessageBox(NULL, "while", "", MB_OK);

	  if(decode_header(&mp->fr,mp->header) == -1) {
	    return MP3_ERR;
	  }
	  
	  mp->framesize = mp->fr.framesize;
	}

	//MessageBox(NULL, "decode header", "", MB_OK);

	if(mp->fr.framesize > mp->bsize)
		return MP3_NEED_MORE;

	wordpointer = mp->bsspace[mp->bsnum] + 512;
	mp->bsnum = (mp->bsnum + 1) & 0x1;
	bitindex = 0;

	len = 0;
	while (len < mp->framesize) {
		int nlen;
		int blen = mp->tail->size - mp->tail->pos;

		if ((mp->framesize - len) <= blen) {
			nlen = mp->framesize-len;
		} else {
			nlen = blen;
                }

		memcpy (wordpointer+len,mp->tail->pnt+mp->tail->pos,nlen);
                len += nlen;
                mp->tail->pos += nlen;
		mp->bsize -= nlen;

                if(mp->tail->pos == mp->tail->size)
                   remove_buf(mp);
	}

	//MessageBox(NULL, "while2", "", MB_OK);

	*done = 0;
	if(mp->fr.error_protection)
		getbits(16);

        switch(mp->fr.lay) {
	case 1:

		do_layer1 (&mp->fr, (uint8_t *) out, done);
		break;
	case 2:

		do_layer2 (&mp->fr, (uint8_t *) out, done);
		break;
	case 3:

		do_layer3 (&mp->fr, (uint8_t *) out, done);
		break;
	}

	//MessageBox(NULL, "do layer", "", MB_OK);

	mp->fsizeold = mp->framesize;
	mp->framesize = 0;

	return MP3_OK;
}


/**
 *
 **/

int set_pointer(long backstep)
{
	uint8_t *bsbufold;

	if(gmp->fsizeold < 0 && backstep > 0) {
	  return MP3_ERR;
	}

	bsbufold = gmp->bsspace[gmp->bsnum] + 512;
	wordpointer -= backstep;

	if (backstep)
		memcpy(wordpointer,bsbufold+gmp->fsizeold-backstep,backstep);

	bitindex = 0;
	return MP3_OK;
}
