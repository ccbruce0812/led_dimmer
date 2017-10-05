#ifndef NET_H
#define NET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <dhcpserver.h>

#include <lwip/inet_chksum.h>
#include <lwip/raw.h>
#include <lwip/icmp.h>
#include <lwip/prot/icmp.h>
#include <lwip/ip4_addr.h>
#include <lwip/ip_addr.h>

#define NET_PING_ID			((u16_t)0xafaf)
#define NET_PING_DATA_SIZE	(32)
#define NET_MAX_LEASE_COUNT	(10)
#define NET_DEF_ADDR(a) 	IP4_ADDR(&a, 192, 168, 254, 254)
#define NET_DEF_GW(a)		IP4_ADDR(&a, 192, 168, 254, 254)
#define NET_DEF_MASK(a)		IP4_ADDR(&a, 255, 255, 255, 0);
#define NET_1ST_ADDR(a)		IP4_ADDR(&a, 192, 168, 254, 100)

typedef struct {
	dhcpserver_lease_t lease;
	bool isAlive;
} PingRec;

static inline void prepareEcho(struct icmp_echo_hdr *iecho, u16_t len, u16_t id, u16_t seqNum) {
	size_t i=0;
	char *tail=((char *)iecho)+sizeof(struct icmp_echo_hdr);

	ICMPH_TYPE_SET(iecho, ICMP_ECHO);
	ICMPH_CODE_SET(iecho, 0);
	iecho->chksum=0;
	iecho->id=id;
	iecho->seqno=seqNum;
	for(i=0;i<len-sizeof(struct icmp_echo_hdr);tail[i]=('a'+i), i++);
	iecho->chksum=inet_chksum(iecho, len);
}

static inline void sendEcho(struct raw_pcb *raw, ip_addr_t *addr, u16_t dataLen, u16_t id, u16_t seqNum) {
	struct pbuf *p=NULL;
	struct icmp_echo_hdr *iecho=NULL;
	size_t len=sizeof(struct icmp_echo_hdr)+dataLen;

	if(!raw || !addr) {
		DBG("Bad argument.\n");
		assert(0);
	}
  
	if(!(p=pbuf_alloc(PBUF_IP, (u16_t)len, PBUF_RAM))) {
		DBG("Failed to invoke pbuf_alloc().\n");
		assert(0);
	}

	if(p->len==p->tot_len &&
		!p->next) {
		iecho=(struct icmp_echo_hdr *)p->payload;
		prepareEcho(iecho, (u16_t)len, id, seqNum);
		raw_sendto(raw, p, addr);
	} else {
		DBG("Unexpected behaviour.\n");
		assert(0);
	}

	pbuf_free(p);
}

#ifdef __cplusplus
}
#endif

#endif
