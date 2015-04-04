#  Project Name
PROJECT=main

# libs dir
LIBDIR=c:\users\matej\cloudstation\arm\stm32\lib

# STM32 stdperiph lib defines
#CDEFS = -DHSE_VALUE=((uint32_t)8000000) -DSTM32F10X_MD_VL
CDEFS = -DHSE_VALUE=((uint32_t)6250000) -DSTM32F10X_MD_VL

#  List of the objects files to be compiled/assembled
OBJECTS=main.o $(LIBDIR)\startup_stm32f10x_md.o

STM_SOURCES = \
$(LIBDIR)\stm32f10x\src\core_cm3.o \
$(LIBDIR)\stm32f10x\src\system_stm32f10x.o \
$(LIBDIR)\stm32f10x\src\stm32f10x_gpio.o \
$(LIBDIR)\stm32f10x\src\stm32f10x_rcc.o \
$(LIBDIR)\stm32f10x\src\stm32f10x_exti.o \
$(LIBDIR)\stm32f10x\src\misc.o \
$(LIBDIR)\stm32f10x\src\stm32f10x_usart.o \
$(LIBDIR)\stm32f10x\src\stm32f10x_spi.o \
$(LIBDIR)\stm32f10x\src\stm32f10x_tim.o \

MAT_SOURCES=\
$(LIBDIR)\mat\circbuf8.o \
$(LIBDIR)\mat\itoa.o \
$(LIBDIR)\mat\serialq.o \
$(LIBDIR)\mat\spi.o

LWIP_SOURCES = \
$(LIBDIR)\lwip-1.4.0\src\core\def.o \
$(LIBDIR)\lwip-1.4.0\src\core\dhcp.o \
$(LIBDIR)\lwip-1.4.0\src\core\dns.o \
$(LIBDIR)\lwip-1.4.0\src\core\init.o \
$(LIBDIR)\lwip-1.4.0\src\core\mem.o \
$(LIBDIR)\lwip-1.4.0\src\core\memp.o \
$(LIBDIR)\lwip-1.4.0\src\core\netif.o \
$(LIBDIR)\lwip-1.4.0\src\core\pbuf.o \
$(LIBDIR)\lwip-1.4.0\src\core\tcp.o \
$(LIBDIR)\lwip-1.4.0\src\core\tcp_in.o \
$(LIBDIR)\lwip-1.4.0\src\core\tcp_out.o \
$(LIBDIR)\lwip-1.4.0\src\core\timers.o \
$(LIBDIR)\lwip-1.4.0\src\core\udp.o \
$(LIBDIR)\lwip-1.4.0\src\core\ipv4\icmp.o \
$(LIBDIR)\lwip-1.4.0\src\core\ipv4\inet.o \
$(LIBDIR)\lwip-1.4.0\src\core\ipv4\inet_chksum.o \
$(LIBDIR)\lwip-1.4.0\src\core\ipv4\ip.o \
$(LIBDIR)\lwip-1.4.0\src\core\ipv4\ip_addr.o \
$(LIBDIR)\lwip-1.4.0\src\netif\etharp.o \
$(LIBDIR)\lwip-1.4.0\src\netif\enc28j60.o \
$(LIBDIR)\lwip-1.4.0\src\netif\mchdrv.o

OBJECTS+=$(STM_SOURCES)
OBJECTS+=$(MAT_SOURCES)
OBJECTS+=$(LWIP_SOURCES)

LSCRIPT=$(LIBDIR)\stm32_flash.ld

OPTIMIZATION = s
DEBUG = dwarf-2
#LISTING = -ahls

#  Compiler Options
GCFLAGS = -g$(DEBUG)
GCFLAGS += $(CDEFS)
GCFLAGS += -O$(OPTIMIZATION)
GCFLAGS += -Wall -std=gnu99 -fno-common -mcpu=cortex-m3 -mthumb
GCFLAGS += -I$(LIBDIR)\stm32f10x\inc -I$(LIBDIR)  -I$(LIBDIR)/lwip-1.4.0/src/include -I$(LIBDIR)/lwip-1.4.0/src/include/ipv4
#GCFLAGS += -Wcast-align -Wcast-qual -Wimplicit -Wpointer-arith -Wswitch
#GCFLAGS += -Wredundant-decls -Wreturn-type -Wshadow -Wunused
LDFLAGS = -mcpu=cortex-m3 -mthumb -O$(OPTIMIZATION) -Wl,-Map=$(PROJECT).map -T$(LSCRIPT)
ASFLAGS = $(LISTING) -mcpu=cortex-m3

#  Compiler/Assembler/Linker Paths
GCC = arm-none-eabi-gcc
AS = arm-none-eabi-as
LD = arm-none-eabi-ld
OBJCOPY = arm-none-eabi-objcopy
REMOVE = del
SIZE = arm-none-eabi-size

#########################################################################

all:: $(PROJECT).hex $(PROJECT).bin stats

$(PROJECT).bin: $(PROJECT).elf
	$(OBJCOPY) -O binary -j .text -j .data $(PROJECT).elf $(PROJECT).bin

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
