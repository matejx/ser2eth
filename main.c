/**
ser2eth using STM32F100, ENC28J60, lwIP and enc28j60driver by chrysn

lwIP:
http://savannah.nongnu.org/projects/lwip/

ebc28j60 driver:
https://gitlab.com/enc28j60driver/enc28j60driver

@file		main.c
@author		Matej Kogovsek
@copyright	GPL v2
*/

#include "stm32f10x.h"

#include "mat/serialq.h"
#include "mat/spi.h"
#include "mat/i2c.h"

#include "enchw.h"

#include "lwip/init.h"
#include "lwip/timeouts.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "netif/mchdrv.h"
#include "netif/etharp.h"
#include "enc28j60.h"

#include <string.h>
#include <ctype.h>

#define ENC28_SPI 1
#define AT_CMD_UART 1
#define AT_CMD_BAUD 38400

// If you want debug messages, define LWIP_DEBUG 1 in lwipopts.h,
// define LWIP_PLATFORM_ASSERT(x) and LWIP_PLATFORM_DIAG(x) in arch/cc.h to use ser_printf(x)
// and select a DBG_UART here (can be same as AT_CMD_UART)
#if LWIP_DEBUG
#define DBG_UART 1
#endif

//-----------------------------------------------------------------------------
//  Global variables
//-----------------------------------------------------------------------------

static uint8_t uart1rxbuf[64];
static uint8_t uart1txbuf[64];

#ifdef DBG_UART
static uint8_t uart2rxbuf[64];
static uint8_t uart2txbuf[64];
#endif

volatile uint32_t msTicks;	// counts SysTicks

static uint8_t at_echo;
static uint8_t lwip_init_done;

static struct netif mchdrv_netif;
static enc_device_t mchdrv_hw;
static ip_addr_t ip = {0};
static ip_addr_t gw = {0};
static ip_addr_t nm = {0};
static struct tcp_pcb* tpcb = 0;
static struct udp_pcb* upcb = 0;

static const uint32_t NV_VAR_ADR = 0x08000000 + (120 * 0x400);

//-----------------------------------------------------------------------------
//  enc28j60 driver required functions
//-----------------------------------------------------------------------------

void enchw_setup(enchw_device_t *dev) { spi_init(ENC28_SPI, SPI_BaudRatePrescaler_128, 0); }
void enchw_select(enchw_device_t *dev) { spi_cs(ENC28_SPI, 0); }
void enchw_unselect(enchw_device_t *dev) { spi_cs(ENC28_SPI, 1); }
uint8_t enchw_exchangebyte(enchw_device_t *dev, uint8_t byte) { return spi_rw(ENC28_SPI, byte); };

//-----------------------------------------------------------------------------
//  newlib required functions
//-----------------------------------------------------------------------------

void _exit(int status)
{
	ser_printf("_exit called!\r\n");
	while( 1 );
}

//-----------------------------------------------------------------------------
//  SysTick handler
//-----------------------------------------------------------------------------

void SysTick_Handler(void)
{
	msTicks++;			// increment counter necessary in _delay_ms()
}

//-----------------------------------------------------------------------------
//  delays number of msTicks (SysTicks)
//-----------------------------------------------------------------------------

void _delay_ms (uint32_t dlyTicks)
{
	uint32_t curTicks = msTicks;
	while ((msTicks - curTicks) < dlyTicks);
}

//-----------------------------------------------------------------------------
//  utility functions
//-----------------------------------------------------------------------------

void ser_putip(const uint8_t n, uint32_t a)
{
	ser_puti(n, (a      ) & 0xff, 10);
	ser_putc(n, '.');
	ser_puti(n, (a >> 8 ) & 0xff, 10);
	ser_putc(n, '.');
	ser_puti(n, (a >> 16) & 0xff, 10);
	ser_putc(n, '.');
	ser_puti(n, (a >> 24) & 0xff, 10);
}

uint32_t udtoi(const char* s) {	// unsigned decimal string to u32
	uint32_t x = 0;

	while( isdigit((int)*s) ) {
		x *= 10;
		x += *s - '0';
		++s;
	}

	return x;
}

uint8_t parse_ip(const char* s, uint32_t* a)
{
	uint8_t i;
	uint32_t ip[4];

	for( i = 0; i < 4; ++i ) {
		ip[i] = udtoi(s);
		if( ip[i] > 255 ) return 1;
		if( 3 == i ) break;
		s = strchr(s, '.');
		if( !s ) return 1;
		++s;
	}

	*a = (ip[0]) | (ip[1] << 8) | (ip[2] << 16) | (ip[3] << 24);

	return 0;
}

uint8_t parse_port(const char* s, uint16_t* a)
{
	uint32_t port = udtoi(s);
	if( (port < 10) || (port > 65536) ) return 1;
	*a = port;
	return 0;
}

uint8_t __fls_wr(uint32_t* page, uint32_t* buf, uint32_t len)
{
//	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

	if( FLASH_COMPLETE != FLASH_ErasePage((uint32_t)page) ) {
		return 1;
	}

	uint32_t i;

	for( i = 0; i < len; ++i ) {
		if( FLASH_COMPLETE != FLASH_ProgramWord((uint32_t)page, *buf) ) {
			return 2;
		}
		++page;
		++buf;
	}

	return 0;
}

uint8_t fls_wr(uint32_t* page, uint32_t* buf, uint32_t len)
{
	if( 0 == memcmp(page, buf, 4*len) ) {
		return 0;
	}

	FLASH_Unlock();
	uint8_t r = __fls_wr(page, buf, len);
	FLASH_Lock();
	if( r ) {
		return r;
	}

	if( 0 != memcmp(page, buf, 4*len) ) {
		return 10;
	}

	return 0;
}

//-----------------------------------------------------------------------------
//  lwIP netif functions
//-----------------------------------------------------------------------------

u32_t sys_now(void)
{
	return msTicks;
}

err_t lwip_tcp_event(void *arg, struct tcp_pcb *pcb, enum lwip_event ev, struct pbuf *p, u16_t size, err_t err)
{
	if( ev == LWIP_EVENT_ACCEPT ) {
		tpcb = pcb;
		ser_puts(AT_CMD_UART, "+LWIP: ACCEPT\r\n");
		LWIP_PLATFORM_DIAG(("tcp: accept\r\n"));
	} else

	if( ev == LWIP_EVENT_SENT ) {
		LWIP_PLATFORM_DIAG(("tcp: sent %d\r\n", size));
	} else

	if( ev == LWIP_EVENT_RECV ) {
		if( p != 0 ) {
			ser_putsn(AT_CMD_UART, p->payload, p->len);
			LWIP_PLATFORM_DIAG(("tcp: recv %d\r\n", p->len));
			tcp_recved(pcb, p->len);
			pbuf_free(p);
		} else {
			tpcb = 0;
			ser_puts(AT_CMD_UART, "+LWIP: CLOSE\r\n");
			LWIP_PLATFORM_DIAG(("tcp: close\r\n"));
			/*if( ERR_OK != tcp_close(pcb) ) {
				LWIP_PLATFORM_DIAG(("tcp_close() fail\r\n"));
			}*/
		}
	} else

	if( ev == LWIP_EVENT_CONNECTED ) {
		ser_puts(AT_CMD_UART, "+LWIP: CONNECT\r\n");
		LWIP_PLATFORM_DIAG(("tcp: connect\r\n"));
	} else

	if( ev == LWIP_EVENT_POLL ) {
		// do nothing
	} else

	if( ev == LWIP_EVENT_ERR ) {
		LWIP_PLATFORM_DIAG(("tcp: error %d occured\r\n", err));
	} else

	{
		LWIP_PLATFORM_DIAG(("tcp: event %d occured\r\n", ev));
	}

	return ERR_OK;
}

void mch_status_callback(struct netif *netif)
{
	ser_puts(AT_CMD_UART, "+LWIP: IFSTAT\r\n");
}

void dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
	ser_puts(AT_CMD_UART, "+LWIPDNS: ");
	if( ipaddr ) {
		ser_putip(AT_CMD_UART, ipaddr->addr);
	} else {
		ser_puts(AT_CMD_UART, "?");
	}
	ser_puts(AT_CMD_UART, "\r\n");
}

void udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
	ser_putsn(AT_CMD_UART, p->payload, p->len);
	LWIP_PLATFORM_DIAG(("udp: recv %d\r\n", p->len));
	pbuf_free(p);
}

//-----------------------------------------------------------------------------
//  AT command processing
//-----------------------------------------------------------------------------

uint8_t proc_at_cmd(const char* s)
{
	if( s[0] == 0 ) { return 1; }

	// general GSM commands

	if( 0 == strcmp(s, "AT") ) {
		return 0;
	}

	if( 0 == strcmp(s, "ATE0") ) {
		at_echo = 0;
		return 0;
	}

	if( 0 == strcmp(s, "ATE1") ) {
		at_echo = 1;
		return 0;
	}

	const char atiresp[] = "MK ser2eth v1.6\r\n";

	if( 0 == strcmp(s, "ATI") ) {
		ser_puts(AT_CMD_UART, atiresp);
		return 0;
	}

	const char atipr[] = "AT+IPR=";

	if( 0 == strncmp(s, atipr, strlen(atipr)) ) {
		s += strlen(atipr);
		uint32_t nbr = udtoi(s);
		if( nbr % 9600 ) return 1;
		ser_puts(AT_CMD_UART, "OK\r\n");
		ser_wait_txe(AT_CMD_UART);
		ser_shutdown(AT_CMD_UART);
		ser_init(AT_CMD_UART, nbr, uart1txbuf, sizeof(uart1txbuf), uart1rxbuf, sizeof(uart1rxbuf));
		return 2;
	}

	// lwIP version

	const char atlwipver[] = "AT+LWIPVER";

	if( 0 == strcmp(s, atlwipver) ) {
		ser_puts(AT_CMD_UART, LWIP_VERSION_STRING);
		ser_puts(AT_CMD_UART, "\r\n");
		return 0;
	}

	// lwIP init

	const char atlwipinit[] = "AT+LWIPINIT";

	if( 0 == strcmp(s, atlwipinit) ) {
		if( lwip_init_done ) return 1;
		lwip_init();
		if( 0 == netif_add(&mchdrv_netif, &ip, &nm, &gw, &mchdrv_hw, mchdrv_init, ethernet_input) ) return 1;
		netif_set_default(&mchdrv_netif);
		netif_set_status_callback(&mchdrv_netif, mch_status_callback);
		netif_set_up(&mchdrv_netif);
		lwip_init_done = 1;
		return 0;
	}

	// all subsequent lwip commands require initialized lwip
	if( !lwip_init_done && (0 != strcmp(s, "AT?")) ) return 1;

	// lwip IF commands

	char atlwipmac[] = "AT+LWIPMAC=?";

	if( 0 == strcmp(s, atlwipmac) ) {
		ser_puts(AT_CMD_UART, "+LWIPMAC: ");
		uint8_t i;
		for( i = 0; i < mchdrv_netif.hwaddr_len; ++i ) {
			ser_puti_lc(AT_CMD_UART, mchdrv_netif.hwaddr[i], 16, 2, '0');
			ser_putc(AT_CMD_UART, ' ');
		}
		ser_puts(AT_CMD_UART, "\r\n");
		return 0;
	}

	const char atlwipdhcp[] = "AT+LWIPDHCP";

	if( 0 == strcmp(s, atlwipdhcp) ) {
		if( ERR_OK != dhcp_start(&mchdrv_netif) ) return 1;
		return 0;
	}

	const char atlwipip[] = "AT+LWIPIP=?";

	if( 0 == strcmp(s, atlwipip) ) {
		ser_puts(AT_CMD_UART, "+LWIPIP: ");
		ser_putip(AT_CMD_UART, mchdrv_netif.ip_addr.addr);
		ser_puts(AT_CMD_UART, "\r\n");
		return 0;
	}

	const char atlwipnm[] = "AT+LWIPNM=?";

	if( 0 == strcmp(s, atlwipnm) ) {
		ser_puts(AT_CMD_UART, "+LWIPNM: ");
		ser_putip(AT_CMD_UART, mchdrv_netif.netmask.addr);
		ser_puts(AT_CMD_UART, "\r\n");
		return 0;
	}

	const char atlwipgw[] = "AT+LWIPGW=?";

	if( 0 == strcmp(s, atlwipgw) ) {
		ser_puts(AT_CMD_UART, "+LWIPGW: ");
		ser_putip(AT_CMD_UART, mchdrv_netif.gw.addr);
		ser_puts(AT_CMD_UART, "\r\n");
		return 0;
	}

	const char atlwipsave[] = "AT+LWIPSAVE";

	if( 0 == strcmp(s, atlwipsave) ) {
		uint32_t ipc[4];
		ipc[0] = mchdrv_netif.ip_addr.addr;
		ipc[1] = mchdrv_netif.netmask.addr;
		ipc[2] = mchdrv_netif.gw.addr;
		ipc[3] = dns_getserver(0)->addr;
		if( 0 ==  fls_wr((uint32_t*)NV_VAR_ADR, ipc, 4) ) {
			return 0;
		}
	}

	const char atlwipload[] = "AT+LWIPLOAD";

	if( 0 == strcmp(s, atlwipload) ) {
		dhcp_stop(&mchdrv_netif);
		netif_set_down(&mchdrv_netif);
		uint32_t* ipcp = (uint32_t*)NV_VAR_ADR;
		ip.addr = *ipcp++;
		nm.addr = *ipcp++;
		gw.addr = *ipcp++;
		netif_set_addr(&mchdrv_netif, &ip, &nm, &gw);
		netif_set_up(&mchdrv_netif);
		ip_addr_t dns0;
		dns0.addr = *ipcp++;
		dns_setserver(0, &dns0);
		return 0;
	}

	// lwIP TCP commands

	const char attcpconnect[] = "AT+TCPCONNECT=";

	if( 0 == strncmp(s, attcpconnect, strlen(attcpconnect)) ) {
		if( tpcb ) return 1;

		s += strlen(attcpconnect);
		ip_addr_t ip;
		if( parse_ip(s, &(ip.addr)) ) return 1;

		uint16_t dstport;
		s = strchr(s, ',');
		if( !s ) return 1;
		++s;
		if( parse_port(s, &dstport) ) return 1;

		uint16_t srcport = 0;
		s = strchr(s, ',');
		if( s ) {
			++s;
			if( parse_port(s, &srcport) ) return 1;
		}

		tpcb = tcp_new();
		if( 0 == tpcb ) return 1;
		if( srcport ) {
			if( ERR_OK != tcp_bind(tpcb, IP_ADDR_ANY, srcport) ) return 1;
		}
		if( ERR_OK != tcp_connect(tpcb, &ip, dstport, 0) ) return 1;

		return 0;
	}

	const char attcplisten[] = "AT+TCPLISTEN=";

	if( 0 == strncmp(s, attcplisten, strlen(attcplisten)) ) {
		if( tpcb ) return 1;

		s += strlen(attcplisten);
		uint16_t srcport;
		if( parse_port(s, &srcport) ) return 1;

		struct tcp_pcb* lpcb = tcp_new();
		if( 0 == lpcb ) return 1;
		if( ERR_OK != tcp_bind(lpcb, IP_ADDR_ANY, srcport) ) return 1;
		tpcb = tcp_listen(lpcb);
		if( 0 == tpcb ) return 1;

		return 0;
	}

	const char attcpsend[] = "AT+TCPSEND";

	if( 0 == strcmp(s, attcpsend) ) {
		if( !tpcb ) return 1;
		_delay_ms(200);
		ser_flush_rxbuf(AT_CMD_UART);
		uint16_t l = tcp_sndbuf(tpcb);
		if( l > 512 ) l = 512;
		ser_puti(AT_CMD_UART, l , 10);
		ser_puts(AT_CMD_UART, ">\r\n");

		uint8_t buf[l];
		uint16_t i = 0;

		while( 1 ) {
			mchdrv_poll(&mchdrv_netif);
			sys_check_timeouts();

			uint8_t d;
			if( ser_getc(AT_CMD_UART, &d) ) {
				if( i >= l ) return 1;

				if( at_echo ) ser_putc(AT_CMD_UART, d);

				if( d == 0x7f && i ) { --i; continue; }

				if( d == 0x1a ) {	// Ctrl-Z
					if( !tpcb ) return 1;
					if( ERR_OK != tcp_write(tpcb, buf, i, TCP_WRITE_FLAG_COPY) ) return 1;
					return 0;
				}

				buf[i++] = d;
			}
		}
	}

	const char attcpoutput[] = "AT+TCPOUTPUT";

	if( 0 == strcmp(s, attcpoutput) ) {
		if( !tpcb ) return 1;
		if( ERR_OK != tcp_output(tpcb) ) return 1;
		return 0;
	}

	const char attcpclose[] = "AT+TCPCLOSE";

	if( 0 == strcmp(s, attcpclose) ) {
		if( !tpcb ) return 1;
		struct tcp_pcb* t = tpcb;
		tpcb = 0;
		if( ERR_OK != tcp_close(t) ) return 1;
		return 0;
	}

	// lwip DNS commands

	const char atdns[] = "AT+DNS=?";

	if( 0 == strcmp(s, atdns) ) {
		uint8_t i;
		for( i = 0; i < DNS_MAX_SERVERS; ++i ) {
			ser_putip(AT_CMD_UART, dns_getserver(i)->addr);
			ser_puts(AT_CMD_UART, "\r\n");
		}
		return 0;
	}

	const char atdnslookup[] = "AT+DNSLOOKUP=";

	if( 0 == strncmp(s, atdnslookup, strlen(atdnslookup)) ) {
		ip_addr_t ip;

		s += strlen(atdnslookup);

		err_t r = dns_gethostbyname(s, &ip, dns_found_cb, 0);

		if( ERR_OK == r ) {
			dns_found_cb(s, &ip, 0);
			return 0;
		}

		if( ERR_INPROGRESS == r ) {
			return 0;
		}

		return 1;
	}

	// lwIP UDP commands

	const char atudpconnect[] = "AT+UDPCONNECT=";

	if( 0 == strncmp(s, atudpconnect, strlen(atudpconnect)) ) {
	  if( upcb ) return 1;

		s += strlen(atudpconnect);
		ip_addr_t ip;
		if( parse_ip(s, &(ip.addr)) ) return 1;

		uint16_t dstport;
		s = strchr(s, ',');
		if( !s ) return 1;
		++s;
		if( parse_port(s, &dstport) ) return 1;

		upcb = udp_new();
		if( !upcb ) return 1;

		if( ERR_OK != udp_connect(upcb, &ip, dstport) ) return 1;

		udp_recv(upcb, udp_recv_callback, 0);

		return 0;
	}

	const char atudplisten[] = "AT+UDPLISTEN=";

	if( 0 == strncmp(s, atudplisten, strlen(atudplisten)) ) {
		if( upcb ) return 1;

		s += strlen(atudplisten);
		uint16_t srcport;
		if( parse_port(s, &srcport) ) return 1;

		upcb = udp_new();
		if( !upcb ) return 1;

		if( ERR_OK != udp_bind(upcb, IP_ADDR_ANY, srcport) ) return 1;

		udp_recv(upcb, udp_recv_callback, 0);

		return 0;
	}

	const char atudpclose[] = "AT+UDPCLOSE";

	if( 0 == strcmp(s, atudpclose) ) {
		if( !upcb ) return 1;
		udp_remove(upcb);
		upcb = 0;
		return 0;
	}

	const char atudpsend[] = "AT+UDPSEND";

	if( 0 == strcmp(s, atudpsend) ) {
		if( !upcb ) return 1;
		_delay_ms(50);
		ser_flush_rxbuf(AT_CMD_UART);
		uint16_t l = 512;

		ser_puti(AT_CMD_UART, l , 10);
		ser_puts(AT_CMD_UART, ">\r\n");

		uint8_t buf[l];
		uint16_t i = 0;

		while( 1 ) {
			mchdrv_poll(&mchdrv_netif);
			sys_check_timeouts();

			uint8_t d;
			if( ser_getc(AT_CMD_UART, &d) ) {
				if( i >= l ) break;

				if( at_echo ) ser_putc(AT_CMD_UART, d);

				if( d == 0x7f && i ) { --i; continue; }

				if( d == 0x1a ) break;

				buf[i++] = d;
			}
		}

		struct pbuf* p = pbuf_alloc(PBUF_TRANSPORT, i, PBUF_REF);
		if( 0 == p ) return 1;
		p->payload = buf;
		err_t e = udp_send(upcb, p);
		pbuf_free(p);
		if( ERR_OK != e ) return 1;
		return 0;
	}

	// help

	if( 0 == strcmp(s, "AT?") ) {
		const char* const atall[] = {
			atipr,
			atlwipver,atlwipinit,atlwipmac,atlwipdhcp,atlwipip,atlwipnm,atlwipgw,atlwipsave,atlwipload,
			attcpconnect,attcplisten,attcpsend,attcpoutput,attcpclose,
			atdns,atdnslookup,
			atudpconnect,atudplisten,atudpclose,atudpsend
		};
		uint8_t i;
		for( i = 0; i < sizeof(atall)/sizeof(char*); ++i ) {
			ser_puts(AT_CMD_UART, atall[i]);
			ser_puts(AT_CMD_UART, "\r\n");
		}
		return 0;
	}

	return 1;
}

//-----------------------------------------------------------------------------
//  MAIN function
//-----------------------------------------------------------------------------

int main(void)
{
	if( SysTick_Config(SystemCoreClock / 1000) ) { // setup SysTick Timer for 1 msec interrupts
		while( 1 );                                  // capture error
	}

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0); // disable preemption
	ser_init(AT_CMD_UART, AT_CMD_BAUD, uart1txbuf, sizeof(uart1txbuf), uart1rxbuf, sizeof(uart1rxbuf));
	#ifdef DBG_UART
	ser_printf_devnum = DBG_UART;
	#if DBG_UART != AT_CMD_UART
	ser_init(DBG_UART, 115200, uart2txbuf, sizeof(uart2txbuf), uart2rxbuf, sizeof(uart2rxbuf));
	#endif
	#endif

	uint32_t* u_id = (uint32_t*)0x1ffff7e8;	// STM32 unique device ID register
	uint32_t mac4 = *u_id ^ *(u_id + 1) ^ *(u_id + 2);

	mchdrv_netif.hwaddr_len = 6;
	mchdrv_netif.hwaddr[0] = 0x02;
	mchdrv_netif.hwaddr[1] = mac4;
	mchdrv_netif.hwaddr[2] = mac4 >> 8;
	mchdrv_netif.hwaddr[3] = mac4 >> 16;
	mchdrv_netif.hwaddr[4] = mac4 >> 24;
	mchdrv_netif.hwaddr[5] = 0x02;

	LWIP_PLATFORM_DIAG(("mcu: reset\r\n"));

	char atbuf[64];
	uint8_t atbuflen = 0;
	at_echo = 1;
	lwip_init_done = 0;

	while( 1 ) {
		if( lwip_init_done ) {
			mchdrv_poll(&mchdrv_netif);
			sys_check_timeouts();
		}

		// at command processing
		uint8_t d;
		if( ser_getc(AT_CMD_UART, &d) ) {

			// echo character
			if( at_echo ) { ser_putc(AT_CMD_UART, d); }

			// buffer overflow guard
			if( atbuflen >= sizeof(atbuf) ) { atbuflen = 0; }

			// execute on enter
			if( (d == '\r') || (d == '\n') ) {
				if( atbuflen ) {
					atbuf[atbuflen] = 0;
					atbuflen = 0;
					uint8_t r = proc_at_cmd(atbuf);
					if( r == 0 ) ser_puts(AT_CMD_UART, "OK\r\n");
					if( r == 1 ) ser_puts(AT_CMD_UART, "ERR\r\n");
				}
			} else
			if( d == 0x7f ) {	// backspace
				if( atbuflen ) { --atbuflen; }
			} else {			// store character
				atbuf[atbuflen++] = toupper(d);
			}
		}
	}
}
