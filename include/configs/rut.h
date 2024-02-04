/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * siemens rut
 * (C) Copyright 2013 Siemens Schweiz AG
 * (C) Heiko Schocher, DENX Software Engineering, hs@denx.de.
 *
 * Based on:
 * U-Boot file:/include/configs/am335x_evm.h
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 */

#ifndef __CONFIG_RUT_H
#define __CONFIG_RUT_H

#include "siemens-am33x-common.h"

#define RUT_IOCTRL_VAL	0x18b
#define DDR_PLL_FREQ	303

 /* Physical Memory Map */
#define CONFIG_MAX_RAM_BANK_SIZE	(256 << 20) /* 256 MiB */

#define CONFIG_FACTORYSET

/* Watchdog */
#define WATCHDOG_TRIGGER_GPIO	14

#ifndef CONFIG_SPL_BUILD

/* Use common default */

/* Default env settings */
#define CONFIG_EXTRA_ENV_SETTINGS \
	"hostname=rut\0" \
	"ubi_off=2048\0"\
	"nand_img_size=0x500000\0" \
	"splashpos=m,m\0" \
	"optargs=fixrtc --no-log consoleblank=0 \0" \
	CONFIG_ENV_SETTINGS_V1 \
	CONFIG_ENV_SETTINGS_NAND_V1 \
	"mmc_dev=0\0" \
	"mmc_root=/dev/mmcblk0p2 rw\0" \
	"mmc_root_fs_type=ext4 rootwait\0" \
	"mmc_load_uimage=" \
		"mmc rescan; " \
		"setenv bootfile uImage;" \
		"fatload mmc ${mmc_dev} ${kloadaddr} ${bootfile}\0" \
	"loadbootenv=fatload mmc ${mmc_dev} ${loadaddr} ${bootenv}\0" \
	"importbootenv=echo Importing environment from mmc ...; " \
		"env import -t $loadaddr $filesize\0" \
	"mmc_args=run bootargs_defaults;" \
		"mtdparts default;" \
		"setenv bootargs ${bootargs} " \
		"root=${mmc_root} ${mtdparts}" \
		"rootfstype=${mmc_root_fs_type} ip=${ip_method} " \
		"eth=${ethaddr} " \
		"\0" \
	"mmc_boot=run mmc_args; " \
		"run mmc_load_uimage; " \
		"bootm ${kloadaddr}\0" \
	""

#ifndef CONFIG_RESTORE_FLASH
/* set to negative value for no autoboot */
#endif

#endif /* CONFIG_SPL_BUILD */

#if defined(CONFIG_VIDEO)
#define CONFIG_VIDEO_DA8XX
#define CONFIG_VIDEO_BMP_LOGO
#define DA8XX_LCD_CNTL_BASE	LCD_CNTL_BASE

#define BOARD_LCD_RESET		115	/* Bank 3 pin 19 */
#define CONFIG_FORMIKE
#define DISPL_PLL_SPREAD_SPECTRUM
#endif

#endif	/* ! __CONFIG_RUT_H */
