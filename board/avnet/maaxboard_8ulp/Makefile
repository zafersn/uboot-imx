# SPDX-License-Identifier: GPL-2.0+

obj-y   += maaxboard_8ulp.o
obj-y  += ../../freescale/common/mmc.o

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
ifdef CONFIG_IMX8ULP_ND_MODE
obj-y += lpddr4_timing_266.o
else
obj-y += lpddr4_timing.o
endif
endif
