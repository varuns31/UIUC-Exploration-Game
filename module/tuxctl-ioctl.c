/* tuxctl-ioctl.c
 *
 * Driver (skeleton) for the mp2 tuxcontrollers for ECE391 at UIUC.
 *
 * Mark Murphy 2006
 * Andrew Ofisher 2007
 * Steve Lumetta 12-13 Sep 2009
 * Puskar Naha 2013
 */

#include <asm/current.h>
#include <asm/uaccess.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/miscdevice.h>
#include <linux/kdev_t.h>
#include <linux/tty.h>
#include <linux/spinlock.h>

#include "tuxctl-ld.h"
#include "tuxctl-ioctl.h"
#include "mtcp.h"

extern char tux_buffer[8];
extern char tux_buffer_reset[8];
extern unsigned char* mem_packet; 
extern unsigned char button[2];

#define debug(str, ...) \
	printk(KERN_DEBUG "%s: " str, __FUNCTION__, ## __VA_ARGS__)

/************************ Protocol Implementation *************************/

/* tuxctl_handle_packet()
 * IMPORTANT : Read the header for tuxctl_ldisc_data_callback() in 
 * tuxctl-ld.c. It calls this function, so all warnings there apply 
 * here as well.
 */
void tuxctl_handle_packet (struct tty_struct* tty, unsigned char* packet)
{
    unsigned a, b, c;

    a = packet[0]; /* Avoid printk() sign extending the 8-bit */
    b = packet[1]; /* values when printing them. */
    c = packet[2];

	if(a==MTCP_POLL_OK)
	{
		button[0]=packet[1];
		button[1]=packet[2];
		return;
	}

    /*printk("packet : %x %x %x\n", a, b, c); */
}

/******** IMPORTANT NOTE: READ THIS BEFORE IMPLEMENTING THE IOCTLS ************
 *                                                                            *
 * The ioctls should not spend any time waiting for responses to the commands *
 * they send to the controller. The data is sent over the serial line at      *
 * 9600 BAUD. At this rate, a byte takes approximately 1 millisecond to       *
 * transmit; this means that there will be about 9 milliseconds between       *
 * the time you request that the low-level serial driver send the             *
 * 6-byte SET_LEDS packet and the time the 3-byte ACK packet finishes         *
 * arriving. This is far too long a time for a system call to take. The       *
 * ioctls should return immediately with success if their parameters are      *
 * valid.                                                                     *
 *                                                                            *
 ******************************************************************************/
int 
tuxctl_ioctl (struct tty_struct* tty, struct file* file, 
	      unsigned cmd, unsigned long arg)
{
    switch (cmd) {
	case TUX_INIT://int a; //tuxinitialise();
	case TUX_BUTTONS:
					{
						int ret=set_button(tty,cmd,arg);
						return ret;
					}
	case TUX_SET_LED:
	case TUX_LED_ACK:return -1;
	case TUX_LED_REQUEST:return -1;
	case TUX_READ_LED:return -1;
	default:
	    return -EINVAL;
    }
}
int set_button(struct tty_struct* tty,unsigned cmd, unsigned long arg)
{
	uint8_t button_val;
	int temp2;

	button_val = (button[1] & 0x1);
	temp2=(button[1] & 0x4);
	temp2=(temp2>>1);
	button_val+=temp2;
	temp2=(button[1] & 0x2);
	temp2=(temp2<<1);
	button_val+=temp2;
	button_val+= (button[1] & 0x8);


	button_val=(button_val<<4);
	button_val+=(button[0] & 0xF);
	unsigned long temp=__copy_to_user ((uint8_t*)arg,(& button_val),1);
	//unsigned long temp=0;
	if(temp>0)
	{
		return -EINVAL;
	}
	else
	return 0;
}
// tuxinitialise:
// {
// 	int a;
// 	return 0;
// }

