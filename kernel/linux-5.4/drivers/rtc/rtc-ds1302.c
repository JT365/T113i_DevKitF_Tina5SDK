/*
 * Dallas DS1302 RTC Support
 *
 *  Copyright (C) 2002 David McCullough
 *  Copyright (C) 2003 - 2007 Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License version 2. See the file "COPYING" in the main directory of
 * this archive for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/rtc.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/bcd.h>

#define DRV_NAME	"rtc-ds1302"
#define DRV_VERSION	"0.1.3"

#define	RTC_CMD_READ	0x81		/* Read command */
#define	RTC_CMD_WRITE	0x80		/* Write command */

#define RTC_ADDR_RAM0	0x20		/* Address of RAM0 */
#define RTC_ADDR_TCR	0x08		/* Address of trickle charge register */
#define	RTC_ADDR_YEAR	0x06		/* Address of year register */
#define	RTC_ADDR_DAY	0x05		/* Address of day of week register */
#define	RTC_ADDR_MON	0x04		/* Address of month register */
#define	RTC_ADDR_DATE	0x03		/* Address of day of month register */
#define	RTC_ADDR_HOUR	0x02		/* Address of hour register */
#define	RTC_ADDR_MIN	0x01		/* Address of minute register */
#define	RTC_ADDR_SEC	0x00		/* Address of second register */

struct ds1302_gpio {
	const char *reset_name;
	unsigned int reset_pin;
	const char *sclk_name;
	unsigned int sclk_pin;
	const char *io_name;
	unsigned int io_pin;
};

static struct ds1302_gpio ds1302_gpio = {
	"DS1302 CS", 0, 
	"DS1302 SCLK", 0,
	"DS1302 IODATA", 0,
};

#ifdef CONFIG_SH_SECUREEDGE5410
#include <asm/rtc.h>
#include <mach/secureedge5410.h>

#define	RTC_RESET	0x1000
#define	RTC_IODATA	0x0800
#define	RTC_SCLK	0x0400

#define set_dp(x)	SECUREEDGE_WRITE_IOPORT(x, 0x1c00)
#define get_dp()	SECUREEDGE_READ_IOPORT()
#define ds1302_set_tx()
#define ds1302_set_rx()

static inline int ds1302_hw_init(void)
{
	return 0;
}

static inline void ds1302_reset(void)
{
	set_dp(get_dp() & ~(RTC_RESET | RTC_IODATA | RTC_SCLK));
}

static inline void ds1302_clock(void)
{
	set_dp(get_dp() | RTC_SCLK);	/* clock high */
	set_dp(get_dp() & ~RTC_SCLK);	/* clock low */
}

static inline void ds1302_start(void)
{
	set_dp(get_dp() | RTC_RESET);
}

static inline void ds1302_stop(void)
{
	set_dp(get_dp() & ~RTC_RESET);
}

static inline void ds1302_txbit(int bit)
{
	set_dp((get_dp() & ~RTC_IODATA) | (bit ? RTC_IODATA : 0));
}

static inline int ds1302_rxbit(void)
{
	return !!(get_dp() & RTC_IODATA);
}

#else
static inline void ds1302_set_tx(void)
{
	gpio_direction_output(ds1302_gpio.io_pin, 1);
}

static inline void ds1302_set_rx(void)
{
	gpio_direction_input(ds1302_gpio.io_pin);
}

static inline void ds1302_reset(void)
{
	gpio_direction_output(ds1302_gpio.reset_pin, 0);
	gpio_direction_output(ds1302_gpio.sclk_pin, 0);
	gpio_direction_input(ds1302_gpio.io_pin);
}

static inline void ds1302_clock(void)
{
	gpio_set_value(ds1302_gpio.sclk_pin, 1);
	udelay(1);
	gpio_set_value(ds1302_gpio.sclk_pin, 0);
	udelay(1);
}

static inline void ds1302_start(void)
{
	gpio_set_value(ds1302_gpio.reset_pin, 1);
}

static inline void ds1302_stop(void)
{
	gpio_set_value(ds1302_gpio.reset_pin, 0);
}

static inline void ds1302_txbit(int bit)
{
	gpio_set_value(ds1302_gpio.io_pin, bit);
}

static inline int ds1302_rxbit(void)
{
	return !!(gpio_get_value(ds1302_gpio.io_pin));
}
#endif

static void ds1302_sendbits(unsigned int val)
{
	int i;

	ds1302_set_tx();

	for (i = 8; (i); i--, val >>= 1) {
		ds1302_txbit(val & 0x1);
		ds1302_clock();
	}
}

static unsigned int ds1302_recvbits(void)
{
	unsigned int val;
	int i;

	ds1302_set_rx();

	for (i = 0, val = 0; (i < 8); i++) {
		val |= (ds1302_rxbit() << i);
		ds1302_clock();
	}

	return val;
}

static unsigned int ds1302_readbyte(unsigned int addr)
{
	unsigned int val;

	ds1302_reset();

	ds1302_start();
	udelay(4);
	ds1302_sendbits(((addr & 0x3f) << 1) | RTC_CMD_READ);
	val = ds1302_recvbits();
	ds1302_stop();

	return val;
}

static void ds1302_writebyte(unsigned int addr, unsigned int val)
{
	ds1302_reset();

	ds1302_start();
	udelay(4);
	ds1302_sendbits(((addr & 0x3f) << 1) | RTC_CMD_WRITE);
	ds1302_sendbits(val);
	ds1302_stop();
}

static int ds1302_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	tm->tm_sec	= bcd2bin(ds1302_readbyte(RTC_ADDR_SEC));
	tm->tm_min	= bcd2bin(ds1302_readbyte(RTC_ADDR_MIN));
	tm->tm_hour	= bcd2bin(ds1302_readbyte(RTC_ADDR_HOUR));
	tm->tm_wday	= bcd2bin(ds1302_readbyte(RTC_ADDR_DAY));
	tm->tm_mday	= bcd2bin(ds1302_readbyte(RTC_ADDR_DATE));
	tm->tm_mon	= bcd2bin(ds1302_readbyte(RTC_ADDR_MON)) - 1;
	tm->tm_year	= bcd2bin(ds1302_readbyte(RTC_ADDR_YEAR));

	if (tm->tm_year < 70)
		tm->tm_year += 100;

	dev_dbg(dev, "%s: tm is secs=%d, mins=%d, hours=%d, "
		"mday=%d, mon=%d, year=%d, wday=%d\n",
		__func__,
		tm->tm_sec, tm->tm_min, tm->tm_hour,
		tm->tm_mday, tm->tm_mon + 1, tm->tm_year, tm->tm_wday);

	return rtc_valid_tm(tm);
}

static int ds1302_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	/* Stop RTC */
	ds1302_writebyte(RTC_ADDR_SEC, ds1302_readbyte(RTC_ADDR_SEC) | 0x80);

	ds1302_writebyte(RTC_ADDR_SEC, bin2bcd(tm->tm_sec));
	ds1302_writebyte(RTC_ADDR_MIN, bin2bcd(tm->tm_min));
	ds1302_writebyte(RTC_ADDR_HOUR, bin2bcd(tm->tm_hour));
	ds1302_writebyte(RTC_ADDR_DAY, bin2bcd(tm->tm_wday));
	ds1302_writebyte(RTC_ADDR_DATE, bin2bcd(tm->tm_mday));
	ds1302_writebyte(RTC_ADDR_MON, bin2bcd(tm->tm_mon + 1));
	ds1302_writebyte(RTC_ADDR_YEAR, bin2bcd(tm->tm_year % 100));

	/* Start RTC */
	ds1302_writebyte(RTC_ADDR_SEC, ds1302_readbyte(RTC_ADDR_SEC) & ~0x80);

	return 0;
}

static ssize_t tcr_show(struct device* dev, struct device_attribute *attr, char *buf) 
{
	unsigned int tcr;

	tcr = ds1302_readbyte(RTC_ADDR_TCR);
	return sprintf(buf, "trickle charge register content: 0x%x\n", tcr);
}

static ssize_t tcr_store(struct device* dev, struct device_attribute *attr, const char *buf, size_t count) 
{
	unsigned int tcr=0;

	sscanf(buf, "%x", &tcr);
	ds1302_writebyte(RTC_ADDR_TCR, tcr);
	return count;
}

static struct device_attribute tcr_attribute = {
	.attr = {
		.name = "trickle_charge",
		.mode = S_IWUSR | S_IRUGO,
	},
	.show = tcr_show,
	.store = tcr_store,
};

static int ds1302_sysfs_create(struct platform_device *pdev, const struct attribute * attr)
{
	int ret;

	ret = sysfs_create_file(&pdev->dev.kobj, attr);
	return ret;
}

static void ds1302_sysfs_remove(struct platform_device *pdev, const struct attribute * attr)
{
	sysfs_remove_file(&pdev->dev.kobj, attr);
}

static int ds1302_rtc_remove(struct platform_device *pdev)
{
	ds1302_sysfs_remove(pdev, &tcr_attribute.attr);
	return 0;
}

static int ds1302_rtc_ioctl(struct device *dev, unsigned int cmd,
			    unsigned long arg)
{
	switch (cmd) {
#ifdef RTC_SET_CHARGE
	case RTC_SET_CHARGE:
	{
		int tcs_val;

		if (copy_from_user(&tcs_val, (int __user *)arg, sizeof(int)))
			return -EFAULT;

		ds1302_writebyte(RTC_ADDR_TCR, (0xa0 | tcs_val * 0xf));
		return 0;
	}
#endif
	}

	return -ENOIOCTLCMD;
}

static struct rtc_class_ops ds1302_rtc_ops = {
	.read_time	= ds1302_rtc_read_time,
	.set_time	= ds1302_rtc_set_time,
	.ioctl		= ds1302_rtc_ioctl,
};

static const struct of_device_id ds1302_dt_ids[] = {
	{ .compatible = "maxim,ds1302", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ds1302_dt_ids);

static int __init ds1302_rtc_probe(struct platform_device *pdev)
{
	int gpio;
	struct rtc_device *rtc;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "Failed to get pdev->dev.of_node");
		return ERR_PTR(-ENODEV);
	}

	gpio = of_get_named_gpio(pdev->dev.of_node, "gpio-ce", 0);
	if (!gpio_is_valid(gpio)) {
		dev_err(&pdev->dev, "Failed to get gpio-ce");
		return ERR_PTR(gpio);
	}
	ds1302_gpio.reset_pin = gpio;
	
	gpio = of_get_named_gpio(pdev->dev.of_node, "gpio-sclk", 0);
	if (!gpio_is_valid(gpio)) {
		dev_err(&pdev->dev, "Failed to get gpio-sclk");
		return ERR_PTR(gpio);
	}
	ds1302_gpio.sclk_pin = gpio;

	gpio = of_get_named_gpio(pdev->dev.of_node, "gpio-io", 0);
	if (!gpio_is_valid(gpio)) {
		dev_err(&pdev->dev, "Failed to get gpio-io");
		return ERR_PTR(gpio);
	}
	ds1302_gpio.io_pin = gpio;

	if (gpio_request(ds1302_gpio.reset_pin, ds1302_gpio.reset_name))
	{
		dev_err(&pdev->dev, "Failed to init reset_pin");
		goto exit_1;
	}

	if (gpio_request(ds1302_gpio.sclk_pin, ds1302_gpio.sclk_name))
	{
		dev_err(&pdev->dev, "Failed to init sclk_pin");
		goto exit_2;
	}

	if (gpio_request(ds1302_gpio.io_pin, ds1302_gpio.io_name))
	{
		dev_err(&pdev->dev, "Failed to init io_pin");
		goto exit_3;
	}

	/* Reset */
	ds1302_reset();

	/* Write a magic value to the DS1302 RAM, and see if it sticks. */
	ds1302_writebyte(RTC_ADDR_RAM0, 0x42);
	if (ds1302_readbyte(RTC_ADDR_RAM0) != 0x42) {
		dev_err(&pdev->dev, "Failed to probe");
		return -ENODEV;
	}

	rtc = devm_rtc_device_register(&pdev->dev,"ds1302", 
					   &ds1302_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc))
	{
		dev_err(&pdev->dev, "Failed to call rtc_device_register");
		return PTR_ERR(rtc);
	}

	platform_set_drvdata(pdev, rtc);
	if (ds1302_sysfs_create(pdev, &tcr_attribute.attr)) {
		dev_err(&pdev->dev, "Failed to register sysfs node");
		return -ENODEV;
	}

	return 0;

exit_3:
	gpio_free(ds1302_gpio.sclk_pin);
exit_2:
	gpio_free(ds1302_gpio.reset_pin);
exit_1:
	return -EINVAL;
}

static struct platform_driver ds1302_platform_driver = {
	.driver		= {
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = ds1302_dt_ids,
	},
	.probe		= ds1302_rtc_probe,
	.remove		= ds1302_rtc_remove,
};

static int __init ds1302_rtc_init(void)
{
	return platform_driver_probe(&ds1302_platform_driver, ds1302_rtc_probe);
}

static void __exit ds1302_rtc_exit(void)
{
	platform_driver_unregister(&ds1302_platform_driver);
}

module_init(ds1302_rtc_init);
module_exit(ds1302_rtc_exit);

MODULE_DESCRIPTION("Dallas DS1302 RTC driver");
MODULE_VERSION(DRV_VERSION);
MODULE_AUTHOR("Paul Mundt, David McCullough");
MODULE_LICENSE("GPL v2");
