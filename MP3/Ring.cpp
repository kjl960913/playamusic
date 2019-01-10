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

/**
 *
 * ring.c
 *
 * Audio Queue.
 * @Author Chavarria Damien.
 *
 */

/*********************************************************************
 *                             INCLUDES                              *
 *********************************************************************/


#include "ring.h"

/*
 * some ring data for
 * the bufferisation
 *
 */

#define RING_SIZE   98000

char                ring[RING_SIZE];
unsigned int        read_pos;
unsigned int        write_pos;

/*********************************************************************
 *                            FUNCTIONS                              *
 *********************************************************************/

void ring_init()
{
  read_pos  = 0;
  write_pos = 0;
}

void ring_read(char *data, int size)
{
  if(write_pos <= read_pos) {
    
    if(read_pos + size < RING_SIZE) {
      memcpy(data, ring + read_pos, size);
      read_pos += size;
    }
    else {
      if(read_pos + size < RING_SIZE + write_pos) {
	
	unsigned int before, after;
	
	before = (RING_SIZE - 1) - read_pos;
	after = size - before;
	
	memcpy(data, ring + read_pos, before);
	memcpy(data + before, ring, after);
	
	read_pos = after;
      }
      else {
	  }
    }
  }
  else {
    if(read_pos + size <= write_pos) {
      memcpy(data, ring + read_pos, size);
      read_pos += size;
    }
    else {
    }
  } 
}

void ring_write(char *data, int size)
{
  if(write_pos >= read_pos) {
    
    if(write_pos + size < RING_SIZE) {
      memcpy(ring + write_pos, data, size);
      write_pos += size;
    }
    else {
      if(write_pos + size < RING_SIZE + read_pos) {
	
	unsigned int before, after;

	before = (RING_SIZE - 1) - write_pos;
	after = size - before;

	memcpy(ring + write_pos, data, before);
	memcpy(ring, data + before, after);


	write_pos = after;
      }
    }
  }
  else {
    if(write_pos + size <= read_pos) {
      memcpy(ring + write_pos, data, size);
      write_pos += size;
    }

    return;
  }
}

int ring_full(int size)
{
  if(write_pos == read_pos)
    return 0;

  if(write_pos > read_pos) {

    if(write_pos + size < read_pos + RING_SIZE)
      return 0;

    return 1;
  }
  else {
    if(write_pos + size < read_pos)
      return 0;

    return 1;
  }
}


