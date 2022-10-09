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

extern char tux_buffer[6];
extern char tux_buffer_reset[6];
extern unsigned char* mem_packet; 
extern unsigned char button[2];

char hex_values[2][16]=
{
{0xE7,0x06,0xCB,0x8F,0x2E,0xAD,0xED,0x86,0xEF,0xAE,0xEE,0x6D,0xE1,0x4F,0xEB,0xE8},
{0XF7,0x16,0xDB,0x9F,0x3E,0xBD,0xFD,0x96,0xFF,0xBE,0xFE,0x7D,0xF1,0x5F,0xFB,0xF8}
};

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

int tuxinitialise(struct tty_struct* tty)
{
	int i=0;
	char init_buffer[2];
	for(i=0;i<8;i++)
	{
		tux_buffer[i]=0;
		tux_buffer_reset[i]=0;
	}
	init_buffer[0]=MTCP_BIOC_ON;
	init_buffer[1]=MTCP_LED_USR;
	tuxctl_ldisc_put(tty,(char const*)init_buffer, 2);
	return 0;
}

int set_button(struct tty_struct* tty,unsigned cmd, unsigned long arg)
{
	uint8_t button_val;
	int temp2;
	unsigned long temp;

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
	temp=__copy_to_user ((uint8_t*)arg,(& button_val),1);
	//unsigned long temp=0;
	if(temp>0)
	{
		return -EINVAL;
	}
	else
	return 0;
}

int set_led_func(struct tty_struct* tty,unsigned long arg)
{
	int first=arg & 0xF;
	int second=arg & 0xF0;
	int third=arg & 0xF00;
	int fourth=arg & 0xF000;
	int decimals[4];
	int masking;
	char first_entry;
	char second_entry;
	char third_entry;
	char fourth_entry;

	second=second>>4;
	third=third>>8;
	fourth=fourth>>12;

	decimals[0]=arg & 0x1000000;
	decimals[1]=arg & 0x2000000;
	decimals[2]=arg & 0x4000000;
	decimals[3]= arg & 0x8000000;

	if(decimals[0]>0)
	{
		first_entry=hex_values[1][first];
	}
	else
	{
		first_entry=hex_values[0][first];
	}

	if(decimals[1]>0)
	{
		second_entry=hex_values[1][second];
	}
	else
	{
		second_entry=hex_values[0][second];
	}

	if(decimals[2]>0)
	{
		third_entry=hex_values[1][third];
	}
	else
	{
		third_entry=hex_values[0][third];
	}

	if(decimals[3]>0)
	{
		fourth_entry=hex_values[1][fourth];
	}
	else
	{
		fourth_entry=hex_values[0][fourth];
	}

	masking=arg & 0xF0000;
	masking=masking>>16;

	if((masking & 0x1)==0)
	{
		first_entry=0;
	}
	if((masking & 0x2)==0)
	{
		second_entry=0;
	}
	if((masking & 0x4)==0)
	{
		third_entry=0;
	}
	if((masking & 0x8)==0)
	{
		fourth_entry=0;
	}

	tux_buffer[0]=MTCP_LED_SET;
	tux_buffer[1]=0xF;
	tux_buffer[2]=first_entry;
	tux_buffer[3]=second_entry;
	tux_buffer[4]=third_entry;
	tux_buffer[5]=fourth_entry;

	tuxctl_ldisc_put(tty,tux_buffer,6);
	return 0;

}

int 
tuxctl_ioctl (struct tty_struct* tty, struct file* file, 
	      unsigned cmd, unsigned long arg)
{
    switch (cmd) {
	case TUX_INIT:return tuxinitialise(tty);
	case TUX_BUTTONS:
					{
						int ret=set_button(tty,cmd,arg);
						return ret;
					}
	case TUX_SET_LED:
					{
						int ret=set_led_func(tty,arg);
						return ret;
					}
	case TUX_LED_ACK:return -1;
	case TUX_LED_REQUEST:return -1;
	case TUX_READ_LED:return -1;
	default:
	    return -EINVAL;
    }
}
// tuxinitialise:
// {
// 	int a;
// 	return 0;
// }

