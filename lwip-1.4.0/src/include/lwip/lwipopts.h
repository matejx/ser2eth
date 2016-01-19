#ifndef MAT_LWIOPTS
#define MAT_LWIOPTS

// platform options
#define NO_SYS                      1
#define MEMP_NUM_SYS_TIMEOUT        5

// memory options
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    1024

// internal memory pool sizes
#define MEMP_NUM_PBUF               8
#define MEMP_NUM_UDP_PCB            2
#define MEMP_NUM_TCP_PCB            1
#define MEMP_NUM_TCP_PCB_LISTEN     1
#define MEMP_NUM_TCP_SEG            8
#define MEMP_NUM_REASSDATA          1
#define MEMP_NUM_FRAG_PBUF          0
#define PBUF_POOL_SIZE              4	// mem

// ARP options
#define LWIP_ARP                    1

// IP options
#define IP_OPTIONS_ALLOWED          0
#define IP_REASSEMBLY               0
#define IP_FRAG                     0

// ICMP options
#define LWIP_ICMP                   1
#define LWIP_BROADCAST_PING         0
#define LWIP_MULTICAST_PING         0

// RAW options
#define LWIP_RAW                    0

// DHCP options
#define LWIP_DHCP                   1

// DNS options
#define LWIP_DNS                    1
#define DNS_TABLE_SIZE              1
#define DNS_MAX_NAME_LENGTH         32

// TCP options
#define TCP_MSS                     256	// mem
#define TCP_SND_BUF                 (2*TCP_MSS)
#define LWIP_EVENT_API              1

// NETIF options
#define LWIP_NETIF_STATUS_CALLBACK  1

// sequential layer API
#define LWIP_NETCONN                0

// socket API
#define LWIP_SOCKET                 0
#define LWIP_TCP_KEEPALIVE          1

// statistics options
#define LWIP_STATS                  0

#define LWIP_CHKSUM_ALGORITHM       2

#define LWIP_DEBUG                  1

#endif
