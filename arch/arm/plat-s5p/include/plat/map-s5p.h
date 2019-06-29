/* linux/arch/arm/plat-s5p/include/plat/map-s5p.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * S5P - Memory map definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

/*
 * 静态映射表的各模块总映射表: （此处的地址皆为虚拟地址）
 * CPU在安排寄存器地址时是按照模块去区分的。每一个模块内部很多寄存器地址是连续的。
 * 该表定义的是各个模块的基地址，具体的寄存器的地址是通过基地址+偏移量来寻址的。
*/

#ifndef __ASM_PLAT_MAP_S5P_H
#define __ASM_PLAT_MAP_S5P_H __FILE__

#define S5P_VA_CHIPID		S3C_ADDR(0x00700000)
// GPIO相关的主映射表路径: arch/arm/mach-s5pv210/include/mach/regs-gpio.h
// GPIO具体寄存器定义路径: arch/arm/mach-s5pv210/include/mach/gpio-bank.h
#define S5P_VA_GPIO			S3C_ADDR(0x00500000)  // 0xFD000000 + 0x00500000 = 0xFD500000
#define S5P_VA_SYSTIMER		S3C_ADDR(0x01200000)
#define S5P_VA_SROMC		S3C_ADDR(0x01100000)
#define S5P_VA_AUDSS		S3C_ADDR(0X01600000)

#define S5P_VA_UART0		(S3C_VA_UART + 0x0)
#define S5P_VA_UART1		(S3C_VA_UART + 0x400)
#define S5P_VA_UART2		(S3C_VA_UART + 0x800)
#define S5P_VA_UART3		(S3C_VA_UART + 0xC00)

#define S3C_UART_OFFSET		(0x400)

#define VA_VIC(x)		(S3C_VA_IRQ + ((x) * 0x10000))
#define VA_VIC0			VA_VIC(0)
#define VA_VIC1			VA_VIC(1)
#define VA_VIC2			VA_VIC(2)
#define VA_VIC3			VA_VIC(3)

#endif /* __ASM_PLAT_MAP_S5P_H */
