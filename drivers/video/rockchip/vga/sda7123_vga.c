#include <linux/ctype.h>
#include <linux/string.h>
#include "firefly_vga.h"
#include <linux/fb.h>
#include <linux/rk_fb.h>
#include <linux/display-sys.h>

#define E(fmt, arg...) printk("<3>!!!%s:%d: " fmt, __FILE__, __LINE__, ##arg)

const struct fb_videomode sda7123_vga_mode[] = {
	//name				refresh		xres	yres	pixclock			h_bp	h_fp	v_bp	v_fp	h_pw	v_pw	polariry	PorI	flag(used for vic)
	{"640x480p@60Hz",	60,			640,	480,	25000000,	48,		16,		33,	   10,		 96,	2,		0,			0,		1	},
	{"800x600p@60Hz",	60,			800,	600,	40000000,	88,	    40,	    23,		1,		128,	4,		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,			0,		0	},	
	{"1024x768p@60Hz",	60,			1024,	768,	65000000,	160,	24,		29,		3,		136,	6,		0,			0,		0	},
	{"1280x720p@60Hz",	60,			1280,	720,	74250000,	220,   110,  	20,		5,		 40,	5,		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,			0,		0	},	
	{"1280x1024p@60Hz",	60,			1280,	1024,	108000000,	248,	48,		38,		1,		112,	3,		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,			0,		6	},
	{"1366x768p@60Hz",	60,			1366,	768,	85500000,	213,	70,		24,		3,		143,	3,		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,			0,		0	},
	{"1440x900p@60Hz",	60,			1440,	900,	106507000,	232,	80,		25,		3,		152,	6,		FB_SYNC_VERT_HIGH_ACT,			0,		0	},
	{"1600x900p@60Hz",	60,			1600,	900,	108000000,	 96,	24,	    96,		1,		 80,	3,		FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,			0,		0	},
	{"1680x1050p@60Hz",	60,			1680,	1050,	146200000,	280,	104,	30,		3,		176,	6,		FB_SYNC_VERT_HIGH_ACT,			0,		0	},
	{"1920x1080p@60Hz",	60,			1920,	1080,	148500000,	148,	88,		36,		4,		 44,	5,		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,			0,		0	},
};


#define MODE_LEN  (ARRAY_SIZE(sda7123_vga_mode))


int get_vga_mode_len()
{
    return MODE_LEN;
}


static struct sda7123_monspecs vga_monspecs;


int firefly_switch_fb(const struct fb_videomode *modedb, int tv_mode)
{	
	struct rk_screen *screen;
	
	if(modedb == NULL)
		return -1;
	screen =  kzalloc(sizeof(struct rk_screen), GFP_KERNEL);
	if(screen == NULL)
		return -1;
		
	printk("%s %d \n",__FUNCTION__,__LINE__);
	
	memset(screen, 0, sizeof(struct rk_screen));	
	/* screen type & face */
	screen->type = SCREEN_RGB;
	screen->face = OUT_P888;
 
	screen->mode = *modedb;
	
	/* Pin polarity */
	if(FB_SYNC_HOR_HIGH_ACT & modedb->sync)
		screen->pin_hsync = 1;
	else
		screen->pin_hsync = 0;
	if(FB_SYNC_VERT_HIGH_ACT & modedb->sync)
		screen->pin_vsync = 1;
	else
		screen->pin_vsync = 0;	
		
	screen->pin_den = 0;
	screen->pin_dclk = 1;
	
	/* Swap rule */
	screen->swap_rb = 0;
	screen->swap_rg = 0;
	screen->swap_gb = 0;
	screen->swap_delta = 0;
	screen->swap_dumy = 0;
	
	/* Operation function*/
	screen->init = NULL;
	screen->standby = NULL;	
	
	rk_fb_switch_screen(screen, 1 , ddev->video_source);
	
	ddev->modeNum = tv_mode;
	
	gpio_set_value(ddev->gpio_sel,ddev->gpio_sel_enable);
	
	kfree(screen);
	
	return 0;
}


int firefly_vga_standby(void)
{
	struct rk_screen screen;
	
	if(vga_monspecs.enable)
		return 0;
	
	gpio_set_value(ddev->gpio_sel, !(ddev->gpio_sel_enable));
	
	screen.type = SCREEN_RGB;
	
	rk_fb_switch_screen(&screen, 0 , ddev->video_source);
	
	ddev->ddc_timer_start = 0;
	
	printk("firefly vga standby\n");

	return 0;
}

int firefly_vga_set_enable(struct rk_display_device *device, int enable)
{
    printk("%s %d enable:%d\n",__FUNCTION__,__LINE__,enable);
	if(vga_monspecs.suspend)
		return 0;
	if(vga_monspecs.enable != enable || vga_monspecs.mode_set != ddev->modeNum)
	{
		if(enable == 0 && vga_monspecs.enable)
		{
			vga_monspecs.enable = 0;
			ddev->ddc_timer_start = 0;
			firefly_vga_standby();
		}
		else if(enable == 1)
		{
			firefly_switch_fb(vga_monspecs.mode, vga_monspecs.mode_set);
			vga_monspecs.enable = 1;
			if(ddev->first_start == 1) {
			    mod_timer(&timer_vga_ddc,jiffies + msecs_to_jiffies(2000));
			    ddev->first_start = 2;
			} else if(ddev->set_mode == 0){
			    mod_timer(&timer_vga_ddc,jiffies + msecs_to_jiffies(400));
			}
			ddev->ddc_timer_start = 1;
		}
	}
	return 0;
}

static int firefly_vga_get_enable(struct rk_display_device *device)
{
    printk("%s %d, %d\n",__FUNCTION__,__LINE__,vga_monspecs.enable);
	return vga_monspecs.enable;
}

static int firefly_vga_get_status(struct rk_display_device *device)
{
    printk("%s %d\n",__FUNCTION__,__LINE__);
	return 1;
}

static int firefly_vga_get_modelist(struct rk_display_device *device, struct list_head **modelist)
{
    printk("%s %d\n",__FUNCTION__,__LINE__);
	*modelist = &(vga_monspecs.modelist);
	return 0;
}

int firefly_vga_set_mode(struct rk_display_device *device, struct fb_videomode *mode)
{
	int i;
	printk("%s %d\n",__FUNCTION__,__LINE__);
    
	for(i = 0; i < MODE_LEN; i++)
	{
		if(fb_mode_is_equal(&sda7123_vga_mode[i], mode))
		{	
			if( ((i + 1) != ddev->modeNum) )
			{
				vga_monspecs.mode_set = i + 1;
				vga_monspecs.mode = (struct fb_videomode *)&sda7123_vga_mode[i];
			}
			return 0;
		}
	}
	
	return -1;
}

static int firefly_vga_get_mode(struct rk_display_device *device, struct fb_videomode *mode)
{
    printk("%s %d\n",__FUNCTION__,__LINE__);
	*mode = *(vga_monspecs.mode);
	return 0;
}

static struct rk_display_ops firefly_vga_display_ops = {
	.setenable = firefly_vga_set_enable,
	.getenable = firefly_vga_get_enable,
	.getstatus = firefly_vga_get_status,
	.getmodelist = firefly_vga_get_modelist,
	.setmode = firefly_vga_set_mode,
	.getmode = firefly_vga_get_mode,
};

static int firefly_display_vga_probe(struct rk_display_device *device, void *devdata)
{
	device->owner = THIS_MODULE;
	strcpy(device->type, "VGA");
	device->name = "firefly_vga";
	device->priority = DISPLAY_PRIORITY_VGA;
	device->property = ddev->property;
	device->priv_data = devdata;
	device->ops = &firefly_vga_display_ops;
	return 1;
}

static struct rk_display_driver display_firefly_vga = {
	.probe = firefly_display_vga_probe,
};

int firefly_register_display_vga(struct device *parent)
{
	int i;
	
	memset(&vga_monspecs, 0, sizeof(struct sda7123_monspecs));
	INIT_LIST_HEAD(&vga_monspecs.modelist);
	for(i = 0; i < MODE_LEN; i++)
		display_add_videomode(&sda7123_vga_mode[i], &vga_monspecs.modelist);

	vga_monspecs.mode = (struct fb_videomode *)&(sda7123_vga_mode[ddev->modeNum - 1]);
	vga_monspecs.mode_set = ddev->modeNum;

	vga_monspecs.ddev = rk_display_device_register(&display_firefly_vga, parent, NULL);
	ddev->vga = &vga_monspecs;
	rk_display_device_enable(vga_monspecs.ddev);
	
	return 0;
}
