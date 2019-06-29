/* arch/arm/mach-s5pv210/include/mach/leds-gpio.h
 *
 * Copyright (c) 2019 Ifan Tsai
 *	https://www.caiyifan.cn
 *	Ifan Tsai <i@caiyifan.cn>
 *
 * S5PV210 - LEDs GPIO connector
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

/* IfanTsai add */

#ifndef __ASM_ARCH_LEDSGPIO_H
#define __ASM_ARCH_LEDSGPIO_H  "leds-gpio.h"

#define S5PV210_LEDF_ACTLOW	    (1 << 0)		/* LED is on when GPIO low */
#define S5PV210_LEDF_TRISTATE	(1 << 1)		/* tristate to turn off */

struct s5pv210_led_platdata {
	unsigned int		 gpio;
	unsigned int		 flags;

	char			*name;
	char			*def_trigger;
};

#endif /* __ASM_ARCH_LEDSGPIO_H */
