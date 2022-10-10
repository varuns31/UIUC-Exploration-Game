/*									tab:8
 *
 * photo.c - photo display functions
 *
 * "Copyright (c) 2011 by Steven S. Lumetta."
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without written agreement is
 * hereby granted, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 * 
 * IN NO EVENT SHALL THE AUTHOR OR THE UNIVERSITY OF ILLINOIS BE LIABLE TO 
 * ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL 
 * DAMAGES ARISING OUT  OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, 
 * EVEN IF THE AUTHOR AND/OR THE UNIVERSITY OF ILLINOIS HAS BEEN ADVISED 
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE AUTHOR AND THE UNIVERSITY OF ILLINOIS SPECIFICALLY DISCLAIM ANY 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE 
 * PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND NEITHER THE AUTHOR NOR
 * THE UNIVERSITY OF ILLINOIS HAS ANY OBLIGATION TO PROVIDE MAINTENANCE, 
 * SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS."
 *
 * Author:	    Steve Lumetta
 * Version:	    3
 * Creation Date:   Fri Sep  9 21:44:10 2011
 * Filename:	    photo.c
 * History:
 *	SL	1	Fri Sep  9 21:44:10 2011
 *		First written (based on mazegame code).
 *	SL	2	Sun Sep 11 14:57:59 2011
 *		Completed initial implementation of functions.
 *	SL	3	Wed Sep 14 21:49:44 2011
 *		Cleaned up code for distribution.
 */


#include <string.h>

#include "assert.h"
#include "modex.h"
#include "photo.h"
#include "photo_headers.h"
#include "world.h"


/* types local to this file (declared in types.h) */

/* 
 * A room photo.  Note that you must write the code that selects the
 * optimized palette colors and fills in the pixel data using them as 
 * well as the code that sets up the VGA to make use of these colors.
 * Pixel data are stored as one-byte values starting from the upper
 * left and traversing the top row before returning to the left of
 * the second row, and so forth.  No padding should be used.
 */
struct photo_t {
    photo_header_t hdr;			/* defines height and width */
    uint8_t        palette[192][3];     /* optimized palette colors */
    uint8_t*       img;                 /* pixel data               */
};

/* 
 * An object image.  The code for managing these images has been given
 * to you.  The data are simply loaded from a file, where they have 
 * been stored as 2:2:2-bit RGB values (one byte each), including 
 * transparent pixels (value OBJ_CLR_TRANSP).  As with the room photos, 
 * pixel data are stored as one-byte values starting from the upper 
 * left and traversing the top row before returning to the left of the 
 * second row, and so forth.  No padding is used.
 */
struct image_t {
    photo_header_t hdr;			/* defines height and width */
    uint8_t*       img;                 /* pixel data               */
};


/* file-scope variables */

/* 
 * The room currently shown on the screen.  This value is not known to 
 * the mode X code, but is needed when filling buffers in callbacks from 
 * that code (fill_horiz_buffer/fill_vert_buffer).  The value is set 
 * by calling prep_room.
 */


/*Octree struct that mantains running sum of least significant bits of r,g,b count of pixels and index before sort*/
typedef struct {
    int count;
    int red;
    int green;
    int blue;
    int index;
}octree_t;

octree_t octree_level_4[4096];//level 4 with 4096 different nodes
octree_t octree_level_2[64];//level 2 with 64 different nodes


/* 
 * comparator
 *   DESCRIPTION: compare values in a struct by frequency
 *   INPUTS: void pointers to be compared
 *   OUTPUTS: none
 *   RETURN VALUE: difference of two values
 *   SIDE EFFECTS: assists qsort to sort the octree array
 */
int comparator(const void* a,const void*b)
{
    octree_t* anew = (octree_t*) a;
    octree_t* bnew = (octree_t*) b;
	/*Returns if a>b*/
    return anew->count - bnew->count;
}

/* 
 * pixelintooctree
 *   DESCRIPTION: puts pixel into a level 4 octree node or finds pixel in index according to mode.
 *   INPUTS: value of pixel and mode (a=0 is put in octree mode , a=1 means find index mode)
 *   OUTPUTS: none
 *   RETURN VALUE: index of pixel in octree_level_4 array
 *   SIDE EFFECTS: added values to global octree_level_4 array
 */
int pixelintooctree(u_int16_t num,int a)
{
    int index;
    int red_val=(num>>11);//find least sig bit of red

    index=(red_val>>1);
    red_val=(red_val & 0x0001);

    int green_val=(num>>5);
    green_val=green_val & (0x003F);

    index=(index<<4);
    index+=(green_val>>2);

    green_val=(green_val & 0x0003);//find least 2 sig bit of green

    int blue_val=num & (0x001F);

    index=(index<<4);
    index+=(blue_val>>1);

    blue_val=(blue_val & 0x01);//find least sig bit of blue

	//push pixel into octree
	if(a==0)
	{
		octree_level_4[index].count++;
    	octree_level_4[index].red+=red_val;
    	octree_level_4[index].blue+=blue_val;
    	octree_level_4[index].green+=green_val;
    	octree_level_4[index].index=index;
		return -1;
	}
	else return index;

}
static const room_t* cur_room = NULL;
//extern void copypalletetoVGA(uint8_t palette[192][3]); 
//extern map_frequency(uint8_t* image ,int size);


/* 
 * fill_horiz_buffer
 *   DESCRIPTION: Given the (x,y) map pixel coordinate of the leftmost 
 *                pixel of a line to be drawn on the screen, this routine 
 *                produces an image of the line.  Each pixel on the line
 *                is represented as a single byte in the image.
 *
 *                Note that this routine draws both the room photo and
 *                the objects in the room.
 *
 *   INPUTS: (x,y) -- leftmost pixel of line to be drawn 
 *   OUTPUTS: buf -- buffer holding image data for the line
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void
fill_horiz_buffer (int x, int y, unsigned char buf[SCROLL_X_DIM])
{
    int            idx;   /* loop index over pixels in the line          */ 
    object_t*      obj;   /* loop index over objects in the current room */
    int            imgx;  /* loop index over pixels in object image      */ 
    int            yoff;  /* y offset into object image                  */ 
    uint8_t        pixel; /* pixel from object image                     */
    const photo_t* view;  /* room photo                                  */
    int32_t        obj_x; /* object x position                           */
    int32_t        obj_y; /* object y position                           */
    const image_t* img;   /* object image                                */

    /* Get pointer to current photo of current room. */
    view = room_photo (cur_room);

    /* Loop over pixels in line. */
    for (idx = 0; idx < SCROLL_X_DIM; idx++) {
        buf[idx] = (0 <= x + idx && view->hdr.width > x + idx ?
		    view->img[view->hdr.width * y + x + idx] : 0);
    }

    /* Loop over objects in the current room. */
    for (obj = room_contents_iterate (cur_room); NULL != obj;
    	 obj = obj_next (obj)) {
	obj_x = obj_get_x (obj);
	obj_y = obj_get_y (obj);
	img = obj_image (obj);

        /* Is object outside of the line we're drawing? */
	if (y < obj_y || y >= obj_y + img->hdr.height ||
	    x + SCROLL_X_DIM <= obj_x || x >= obj_x + img->hdr.width) {
	    continue;
	}

	/* The y offset of drawing is fixed. */
	yoff = (y - obj_y) * img->hdr.width;

	/* 
	 * The x offsets depend on whether the object starts to the left
	 * or to the right of the starting point for the line being drawn.
	 */
	if (x <= obj_x) {
	    idx = obj_x - x;
	    imgx = 0;
	} else {
	    idx = 0;
	    imgx = x - obj_x;
	}

	/* Copy the object's pixel data. */
	for (; SCROLL_X_DIM > idx && img->hdr.width > imgx; idx++, imgx++) {
	    pixel = img->img[yoff + imgx];

	    /* Don't copy transparent pixels. */
	    if (OBJ_CLR_TRANSP != pixel) {
		buf[idx] = pixel;
	    }
	}
    }
}


/* 
 * fill_vert_buffer
 *   DESCRIPTION: Given the (x,y) map pixel coordinate of the top pixel of 
 *                a vertical line to be drawn on the screen, this routine 
 *                produces an image of the line.  Each pixel on the line
 *                is represented as a single byte in the image.
 *
 *                Note that this routine draws both the room photo and
 *                the objects in the room.
 *
 *   INPUTS: (x,y) -- top pixel of line to be drawn 
 *   OUTPUTS: buf -- buffer holding image data for the line
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void
fill_vert_buffer (int x, int y, unsigned char buf[SCROLL_Y_DIM])
{
    int            idx;   /* loop index over pixels in the line          */ 
    object_t*      obj;   /* loop index over objects in the current room */
    int            imgy;  /* loop index over pixels in object image      */ 
    int            xoff;  /* x offset into object image                  */ 
    uint8_t        pixel; /* pixel from object image                     */
    const photo_t* view;  /* room photo                                  */
    int32_t        obj_x; /* object x position                           */
    int32_t        obj_y; /* object y position                           */
    const image_t* img;   /* object image                                */

    /* Get pointer to current photo of current room. */
    view = room_photo (cur_room);

    /* Loop over pixels in line. */
    for (idx = 0; idx < SCROLL_Y_DIM; idx++) {
        buf[idx] = (0 <= y + idx && view->hdr.height > y + idx ?
		    view->img[view->hdr.width * (y + idx) + x] : 0);
    }

    /* Loop over objects in the current room. */
    for (obj = room_contents_iterate (cur_room); NULL != obj;
    	 obj = obj_next (obj)) {
	obj_x = obj_get_x (obj);
	obj_y = obj_get_y (obj);
	img = obj_image (obj);

        /* Is object outside of the line we're drawing? */
	if (x < obj_x || x >= obj_x + img->hdr.width ||
	    y + SCROLL_Y_DIM <= obj_y || y >= obj_y + img->hdr.height) {
	    continue;
	}

	/* The x offset of drawing is fixed. */
	xoff = x - obj_x;

	/* 
	 * The y offsets depend on whether the object starts below or 
	 * above the starting point for the line being drawn.
	 */
	if (y <= obj_y) {
	    idx = obj_y - y;
	    imgy = 0;
	} else {
	    idx = 0;
	    imgy = y - obj_y;
	}

	/* Copy the object's pixel data. */
	for (; SCROLL_Y_DIM > idx && img->hdr.height > imgy; idx++, imgy++) {
	    pixel = img->img[xoff + img->hdr.width * imgy];

	    /* Don't copy transparent pixels. */
	    if (OBJ_CLR_TRANSP != pixel) {
		buf[idx] = pixel;
	    }
	}
    }
}


/* 
 * image_height
 *   DESCRIPTION: Get height of object image in pixels.
 *   INPUTS: im -- object image pointer
 *   OUTPUTS: none
 *   RETURN VALUE: height of object image im in pixels
 *   SIDE EFFECTS: none
 */
uint32_t 
image_height (const image_t* im)
{
    return im->hdr.height;
}


/* 
 * image_width
 *   DESCRIPTION: Get width of object image in pixels.
 *   INPUTS: im -- object image pointer
 *   OUTPUTS: none
 *   RETURN VALUE: width of object image im in pixels
 *   SIDE EFFECTS: none
 */
uint32_t 
image_width (const image_t* im)
{
    return im->hdr.width;
}

/* 
 * photo_height
 *   DESCRIPTION: Get height of room photo in pixels.
 *   INPUTS: p -- room photo pointer
 *   OUTPUTS: none
 *   RETURN VALUE: height of room photo p in pixels
 *   SIDE EFFECTS: none
 */
uint32_t 
photo_height (const photo_t* p)
{
    return p->hdr.height;
}


/* 
 * photo_width
 *   DESCRIPTION: Get width of room photo in pixels.
 *   INPUTS: p -- room photo pointer
 *   OUTPUTS: none
 *   RETURN VALUE: width of room photo p in pixels
 *   SIDE EFFECTS: none
 */
uint32_t 
photo_width (const photo_t* p)
{
    return p->hdr.width;
}


/* 
 * prep_room
 *   DESCRIPTION: Prepare a new room for display.  You might want to set
 *                up the VGA palette registers according to the color
 *                palette that you chose for this room.
 *   INPUTS: r -- pointer to the new room
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: changes recorded cur_room for this file
 */
void
prep_room (const room_t* r)
{
	photo_t* view =room_photo(r);
	//map_frequency(view->img,(view->hdr.height*view->hdr.width));
	
	copypalletetoVGA((uint8_t*) view->palette);//Add created palette to vga palette

    /* Record the current room. */
    cur_room = r;
}


/* 
 * read_obj_image
 *   DESCRIPTION: Read size and pixel data in 2:2:2 RGB format from a
 *                photo file and create an image structure from it.
 *   INPUTS: fname -- file name for input
 *   OUTPUTS: none
 *   RETURN VALUE: pointer to newly allocated photo on success, or NULL
 *                 on failure
 *   SIDE EFFECTS: dynamically allocates memory for the image
 */
image_t*
read_obj_image (const char* fname)
{
    FILE*    in;		/* input file               */
    image_t* img = NULL;	/* image structure          */
    uint16_t x;			/* index over image columns */
    uint16_t y;			/* index over image rows    */
    uint8_t  pixel;		/* one pixel from the file  */

    /* 
     * Open the file, allocate the structure, read the header, do some
     * sanity checks on it, and allocate space to hold the image pixels.
     * If anything fails, clean up as necessary and return NULL.
     */
    if (NULL == (in = fopen (fname, "r+b")) ||
	NULL == (img = malloc (sizeof (*img))) ||
	NULL != (img->img = NULL) || /* false clause for initialization */
	1 != fread (&img->hdr, sizeof (img->hdr), 1, in) ||
	MAX_OBJECT_WIDTH < img->hdr.width ||
	MAX_OBJECT_HEIGHT < img->hdr.height ||
	NULL == (img->img = malloc 
		 (img->hdr.width * img->hdr.height * sizeof (img->img[0])))) {
	if (NULL != img) {
	    if (NULL != img->img) {
	        free (img->img);
	    }
	    free (img);
	}
	if (NULL != in) {
	    (void)fclose (in);
	}
	return NULL;
    }

    /* 
     * Loop over rows from bottom to top.  Note that the file is stored
     * in this order, whereas in memory we store the data in the reverse
     * order (top to bottom).
     */
    for (y = img->hdr.height; y-- > 0; ) {

	/* Loop over columns from left to right. */
	for (x = 0; img->hdr.width > x; x++) {

	    /* 
	     * Try to read one 8-bit pixel.  On failure, clean up and 
	     * return NULL.
	     */
	    if (1 != fread (&pixel, sizeof (pixel), 1, in)) {
		free (img->img);
		free (img);
	        (void)fclose (in);
		return NULL;
	    }

	    /* Store the pixel in the image data. */
	    img->img[img->hdr.width * y + x] = pixel;
	}
    }

    /* All done.  Return success. */
    (void)fclose (in);
    return img;
}


/* 
 * read_photo
 *   DESCRIPTION: Read size and pixel data in 5:6:5 RGB format from a
 *                photo file and create a photo structure from it.
 *                Code provided simply maps to 2:2:2 RGB.  You must
 *                replace this code with palette color selection, and
 *                must map the image pixels into the palette colors that
 *                you have defined.
 *   INPUTS: fname -- file name for input
 *   OUTPUTS: none
 *   RETURN VALUE: pointer to newly allocated photo on success, or NULL
 *                 on failure
 *   SIDE EFFECTS: dynamically allocates memory for the photo
 */
photo_t*
read_photo (const char* fname)
{
    FILE*    in;	/* input file               */
    photo_t* p = NULL;	/* photo structure          */
    uint16_t x;		/* index over image columns */
    uint16_t y;		/* index over image rows    */
    uint16_t pixel;	/* one pixel from the file  */
	int i;

    /* 
     * Open the file, allocate the structure, read the header, do some
     * sanity checks on it, and allocate space to hold the photo pixels.
     * If anything fails, clean up as necessary and return NULL.
     */
    if (NULL == (in = fopen (fname, "r+b")) ||
	NULL == (p = malloc (sizeof (*p))) ||
	NULL != (p->img = NULL) || /* false clause for initialization */
	1 != fread (&p->hdr, sizeof (p->hdr), 1, in) ||
	MAX_PHOTO_WIDTH < p->hdr.width ||
	MAX_PHOTO_HEIGHT < p->hdr.height ||
	NULL == (p->img = malloc 
		 (p->hdr.width * p->hdr.height * sizeof (p->img[0])))) {
	if (NULL != p) {
	    if (NULL != p->img) {
	        free (p->img);
	    }
	    free (p);
	}
	if (NULL != in) {
	    (void)fclose (in);
	}
	return NULL;
    }

	//initialise global octree arrays to 0
	for(i=0;i<4096;i++)
	{
		octree_level_4[i].index=i;
		octree_level_4[i].red=0;
		octree_level_4[i].green=0;
		octree_level_4[i].blue=0;
		octree_level_4[i].count=0;
	}
	for(i=0;i<192;i++)
	{
		p->palette[i][0]=0;
		p->palette[i][1]=0;
		p->palette[i][2]=0;
	}

    /* 
     * Loop over rows from bottom to top.  Note that the file is stored
     * in this order, whereas in memory we store the data in the reverse
     * order (top to bottom).
     */
    for (y = p->hdr.height; y-- > 0; ) {

	/* Loop over columns from left to right. */
	for (x = 0; p->hdr.width > x; x++) {

	    /* 
	     * Try to read one 16-bit pixel.  On failure, clean up and 
	     * return NULL.
	     */
	    if (1 != fread (&pixel, sizeof (pixel), 1, in)) {
		free (p->img);
		free (p);
	        (void)fclose (in);
		return NULL;

	    }

		//add pixel to octree
		pixelintooctree(pixel,0);
	    /* 
	     * 16-bit pixel is coded as 5:6:5 RGB (5 bits red, 6 bits green,
	     * and 6 bits blue).  We change to 2:2:2, which we've set for the
	     * game objects.  You need to use the other 192 palette colors
	     * to specialize the appearance of each photo.
	     *
	     * In this code, you need to calculate the p->palette values,
	     * which encode 6-bit RGB as arrays of three uint8_t's.  When
	     * the game puts up a photo, you should then change the palette 
	     * to match the colors needed for that photo.
	     */
	    p->img[p->hdr.width * y + x] = (((pixel >> 14) << 4) |
					    (((pixel >> 9) & 0x3) << 2) |
					    ((pixel >> 3) & 0x3));
	}
    }

	//unsorted octree array for map like efficiency
	octree_t map_help[4096];
	for(i=0;i<4096;i++)
	{
		map_help[i]=octree_level_4[i];
	}

	//sort array based on count
	qsort(octree_level_4,4096,sizeof(octree_t),comparator);

	for(i=0;i<128;i++)
	{
		int count=octree_level_4[4095-i].count;
		count=count/2;
		int val=octree_level_4[4095-i].index;
		int red=(val>>8);
		red=(red<<1);
		if(octree_level_4[4095-i].red >=count)
		{
			red++;
		}
		//red=red/count;
		int green=(val>>4);
		green=green & 0xF;
		green=(green<<2);
		if(count !=0)
		{
			green=green + (octree_level_4[4095-i].green/(2*count));
		}
		//green=green/count

		int blue=val & 0xF;
		blue=(blue<<1);
		if(octree_level_4[4095-i].blue >=count)
		{
			blue++;
		}
		//blue=blue/count;

		p->palette[i][0]=(red<<1);//push red values in palette according to calculated average
		p->palette[i][1]=green;//push green values in palette according to calculated average
		p->palette[i][2]=(blue<<1);//push blue values in palette according to calculated average
	}

	//initialise level_2 octree array to 0
	for(i=0;i<64;i++)
	{
		octree_level_2[i].index=i;
		octree_level_2[i].red=0;
		octree_level_2[i].green=0;
		octree_level_2[i].blue=0;
		octree_level_2[i].count=0;
	}

	//fill level_2 array using level_4 sorted array
	for(i=128;i<4096;i++)
	{
		int temp;
		int ind2;
		int ind1=octree_level_4[4095-i].index;

		ind2=(ind1>>10);
		ind2=(ind2<<2);
		temp=(ind1>>6);
		temp=temp&0x03;
		ind2+=temp;
		ind2=(ind2<<2);
		temp=(ind1>>2);
		temp=temp&0x03;
		ind2+=temp;

		if(octree_level_2[ind2].count==0x0)
		{
			int red=(ind1>>7);
			red=red & 0x06;
			int green=(ind1>>2);
			green=green & 0x0C;
			int blue= ind1 & 0x03;
			blue=(blue<<1);

			int count=octree_level_4[4095-i].count;

			if(count!=0)
			{
				blue*=count;
				green*=count;
				red*=count;
			}
			
			//update pixel into octree level_2 which is in the lower 3969 level4 values
			octree_level_2[ind2].red=red+octree_level_4[4095-i].red;
			octree_level_2[ind2].green=green+octree_level_4[4095-i].green;
			octree_level_2[ind2].blue=blue+octree_level_4[4095-i].blue;
			octree_level_2[ind2].count=octree_level_4[4095-i].count;
		}
		else
		{
			int red=(ind1>>7);
			red=red & 0x06;
			int green=(ind1>>2);
			green=green & 0x0C;
			int blue= ind1 & 0x03;
			blue=(blue<<1);

			int count=octree_level_4[4095-i].count;
			blue*=count;
			green*=count;
			red*=count;
			

			red=red+octree_level_4[4095-i].red;
			green=green+octree_level_4[4095-i].green;
			blue=blue+octree_level_4[4095-i].blue;

			//update pixel into octree level_2 which is in the lower 3969 level4 values
			octree_level_2[ind2].red=red+octree_level_2[ind2].red;
			octree_level_2[ind2].green=green+octree_level_2[ind2].green;
			octree_level_2[ind2].blue=blue+octree_level_2[ind2].blue;
			octree_level_2[ind2].count+=octree_level_4[4095-i].count;
		}
	}

	//fill level_2 palette using averages in level2 nodes
	for(i=0;i<64;i++)
	{
		int red_ind=(i>>4);
		int green_ind=0;
		int blue_ind=0;
		int red=octree_level_2[i].red;
		int count=octree_level_2[i].count;
		
		if(count == 0 )
		{
			red_ind=0;
			green_ind=0;
			blue_ind=0;

		}
		else
		{
			red=red/count;
			red_ind=red_ind<<4;
			red=red<<1;
			red_ind=red_ind+red;

			green_ind=(i>>2);
			green_ind= green_ind & 0x03;
			int green=octree_level_2[i].green;
			green=green/count;
			green_ind=green_ind<<4;
			green_ind=green_ind+green;

			blue_ind=(i & 0x03);
			int blue=octree_level_2[i].blue;
			blue=blue/count;
			blue_ind=blue_ind<<4;
			blue=blue<<1;
			blue_ind=blue_ind+blue;

		}
		
		//fill level 2 palette of last 64 values
		p->palette[128+i][0]=red_ind;
		p->palette[128+i][1]=green_ind;
		p->palette[128+i][2]=blue_ind;
	}

	// p->palette[1][0]=0x00;
	// p->palette[1][1]=0x00;
	// p->palette[1][2]=0x00;

	//reset file pointer to start of file
	fseek(in,0,SEEK_SET);
	
	for (y = p->hdr.height; y-- > 0; ) {

	/* Loop over columns from left to right. */
	for (x = 0; p->hdr.width > x; x++) {

	    /* 
	     * Try to read one 16-bit pixel.  On failure, clean up and 
	     * return NULL.
	     */
	    if (1 != fread (&pixel, sizeof (pixel), 1, in)) {
		free (p->img);
		free (p);
	        (void)fclose (in);
		return NULL;
	    }

		//find index of pixel to check if it is level4 top 128 nodes
		int index = pixelintooctree(pixel,1);

		//Lowest count of level4 that is in top 128 for comparison
		int count= octree_level_4[4095-128].count;

		

		//p->img[p->hdr.width * y + x]=64+1;
	    /* 
	     * 16-bit pixel is coded as 5:6:5 RGB (5 bits red, 6 bits green,
	     * and 6 bits blue).  We change to 2:2:2, which we've set for the
	     * game objects.  You need to use the other 192 palette colors
	     * to specialize the appearance of each photo.
	     *
	     * In this code, you need to calculate the p->palette values,
	     * which encode 6-bit RGB as arrays of three uint8_t's.  When
	     * the game puts up a photo, you should then change the palette 
	     * to match the colors needed for that photo.
	     */
	    //p->img[p->hdr.width * y + x] = (((pixel >> 14) << 4) |
					    // (((pixel >> 9) & 0x3) << 2) |
					    // ((pixel >> 3) & 0x3));
		if(map_help[index].count>count)
		{
			for(i=0;i<128;i++)
			{
				if(octree_level_4[4095-i].index==index)
				{
					//map pixel to level_4 corresponding value
					p->img[p->hdr.width * y + x -2]=64+i;
					break;
				}
			}
		}
		else
		{
			int red=(pixel>>14);
			red=(red<<4);

			int green=(pixel>>9);
			green=green & 0x03;
			green=(green<<2);

			int blue=(pixel>>3);
			blue=blue & 0x03;

			int index= red+blue+green;

			//map pixel to level_2 corresponding value
			p->img[p->hdr.width * y + x -2]=index+192;
		}
}
    }
    /* All done.  Return success. */
    (void)fclose (in);
    return p;
}


