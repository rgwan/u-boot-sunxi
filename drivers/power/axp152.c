/*
 * (C) Copyright 2012
 * Henrik Nordstrom <henrik@henriknordstrom.net>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <i2c.h>
#include <axp152.h>

#define AXP152_I2C_ADDR		0x30
#define AXP152_CHIP_ID		0x05

enum axp152_reg {
	AXP152_CHIP_VERSION = 0x3,
	AXP152_DCDC2_VOLTAGE = 0x23,
	AXP152_SHUTDOWN = 0x32,
};

static int axp152_write(enum axp152_reg reg, u8 val)
{
	return i2c_write(AXP152_I2C_ADDR, reg, 1, &val, 1);
}

static int axp152_read(enum axp152_reg reg, u8 *val)
{
	return i2c_read(AXP152_I2C_ADDR, reg, 1, val, 1);
}

int axp152_set_dcdc2(int mvolt)
{
	int cfg = (mvolt - 700) / 25;

	if (cfg < 0)
		cfg = 0;
	if (cfg > (1 << 6) - 1)
		cfg = (1 << 6) - 1;

	return axp152_write(AXP152_DCDC2_VOLTAGE, cfg);
}

void axp152_poweroff(void)
{
	u8 val;

	if (axp152_read(AXP152_SHUTDOWN, &val) != 0)
		return;

	val |= 1 << 7;

	if (axp152_write(AXP152_SHUTDOWN, val) != 0)
		return;

	udelay(10000);		/* wait for power to drain */
}

int axp152_init(void)
{
	u8 ver;
	int rc;

	rc = axp152_read(AXP152_CHIP_VERSION, &ver);
	if (rc)
		return rc;

	if (ver != AXP152_CHIP_ID)
		return -1;

	return 0;
}
