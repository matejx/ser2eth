#  Project Name
PROJECT=main

# libs dir
LIBDIR=../../lib
LWIPDIR=lwip-2.0.3

# STM32 stdperiph lib defines
#CDEFS=-DHSE_VALUE=8000000 -DSTM32F10X_MD_VL -DUSE_STDPERIPH_DRIVER
CDEFS = -DHSE_VALUE=6250000 -DSTM32F10X_MD_VL -DUSE_STDPERIPH_DRIVER

#  List of the objects files to be compiled/assembled
OBJECTS=main.o \
enc28j60/enc28j60.o \
enc28j60/mchdrv.o

CMSIS_SOURCES=\
$(LIBDIR)/cmsis/startup_stm32f10x_md.o \
$(LIBDIR)/cmsis/system_stm32f10x.o
#$(LIBDIR)\cmsis\core_cm3.o

STM_SOURCES=\
$(LIBDIR)/stm32f10x/src/stm32f10x_gpio.o \
$(LIBDIR)/stm32f10x/src/stm32f10x_rcc.o \
$(LIBDIR)/stm32f10x/src/stm32f10x_exti.o \
$(LIBDIR)/stm32f10x/src/misc.o \
$(LIBDIR)/stm32f10x/src/stm32f10x_usart.o \
$(LIBDIR)/stm32f10x/src/stm32f10x_spi.o \
$(LIBDIR)/stm32f10x/src/stm32f10x_tim.o \
$(LIBDIR)/stm32f10x/src/stm32f10x_flash.o

MAT_SOURCES=\
$(LIBDIR)/mat/circbuf8.o \
$(LIBDIR)/mat/itoa.o \
$(LIBDIR)/mat/serialq.o \
$(LIBDIR)/mat/spi.o

LWIP_SOURCES = \
$(LWIPDIR)/src/api/err.o \
$(LWIPDIR)/src/core/def.o \
$(LWIPDIR)/src/core/dns.o \
$(LWIPDIR)/src/core/inet_chksum.o \
$(LWIPDIR)/src/core/init.o \
$(LWIPDIR)/src/core/ip.o \
$(LWIPDIR)/src/core/mem.o \
$(LWIPDIR)/src/core/memp.o \
$(LWIPDIR)/src/core/netif.o \
$(LWIPDIR)/src/core/pbuf.o \
$(LWIPDIR)/src/core/tcp.o \
$(LWIPDIR)/src/core/tcp_in.o \
$(LWIPDIR)/src/core/tcp_out.o \
$(LWIPDIR)/src/core/timeouts.o \
$(LWIPDIR)/src/core/udp.o \
$(LWIPDIR)/src/core/ipv4/dhcp.o \
$(LWIPDIR)/src/core/ipv4/etharp.o \
$(LWIPDIR)/src/core/ipv4/icmp.o \
$(LWIPDIR)/src/core/ipv4/ip4.o \
$(LWIPDIR)/src/core/ipv4/ip4_addr.o \
$(LWIPDIR)/src/netif/ethernet.o

OBJECTS+=$(CMSIS_SOURCES)
OBJECTS+=$(STM_SOURCES)
OBJECTS+=$(MAT_SOURCES)
OBJECTS+=$(LWIP_SOURCES)

LSCRIPT=$(LIBDIR)/cmsis/stm32f100xb.ld

OPTIMIZATION = s
DEBUG = dwarf-2
#LISTING = -ahls

#  Compiler Options
GCFLAGS = -g$(DEBUG)
GCFLAGS += $(CDEFS)
GCFLAGS += -O$(OPTIMIZATION)
GCFLAGS += -Wall -std=gnu99 -fno-common -mcpu=cortex-m3 -mthumb
GCFLAGS += -I$(LIBDIR)/stm32f10x/inc -I$(LIBDIR)/cmsis -I$(LIBDIR) -I$(LWIPDIR)/src/include -Ienc28j60/include
#GCFLAGS += -Wcast-align -Wcast-qual -Wimplicit -Wpointer-arith -Wswitch
#GCFLAGS += -Wredundant-decls -Wreturn-type -Wshadow -Wunused
LDFLAGS = -mcpu=cortex-m3 -mthumb -O$(OPTIMIZATION) -Wl,-Map=$(PROJECT).map -T$(LSCRIPT)
ASFLAGS = $(LISTING) -mcpu=cortex-m3

#  Compiler/Assembler/Linker Paths
GCC = arm-none-eabi-gcc
AS = arm-none-eabi-as
LD = arm-none-eabi-ld
OBJCOPY = arm-none-eabi-objcopy
ifeq ($(OS), Windows_NT)
REMOVE = rm.py -f
else
REMOVE = rm -f
endif
SIZE = arm-none-eabi-size

#########################################################################

all: $(PROJECT).hex $(PROJECT).bin stats

$(PROJECT).bin: $(PROJECT).elf
#	$(OBJCOPY) -O binary -j .text -j .data $(PROJECT).elf $(PROJECT).bin
	$(OBJCOPY) -R .stack -O binary $(PROJECT).elf $(PROJECT).bin

$(PROJECT).hex: $(PROJECT).elf
	$(OBJCOPY) -R .stack -O ihex $(PROJECT).elf $(PROJECT).hex

$(PROJECT).elf: $(OBJECTS)
	$(GCC) $(LDFLAGS) $(OBJECTS) -o $(PROJECT).elf

stats: $(PROJECT).elf
	$(SIZE) $(PROJECT).elf

clean:
	$(REMOVE) $(OBJECTS)
	$(REMOVE) $(PROJECT).hex
	$(REMOVE) $(PROJECT).elf
	$(REMOVE) $(PROJECT).map
	$(REMOVE) $(PROJECT).bin

program: $(PROJECT).hex
	st-flash --format ihex write $(PROJECT).hex

#########################################################################
#  Default rules to compile .c and .cpp file to .o
#  and assemble .s files to .o

.c.o :
	$(GCC) $(GCFLAGS) -c $< -o $@

.cpp.o :
	$(GCC) $(GCFLAGS) -c $< -o $@

.s.o :
	$(AS) $(ASFLAGS) -o $@ $<
#	$(AS) $(ASFLAGS) -o $(PROJECT)_crt.o $< > $(PROJECT)_crt.lst

#########################################################################
-include $(shell mkdir .dep) $(wildcard .dep/*)
