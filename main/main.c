#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include <esp8266.h>
#include <esp/uart.h>

#include <etstimer.h>
#include <espressif/esp_common.h>
#include <espressif/osapi.h>

#include <dhcpserver.h>
#include <esp_spiffs.h>

#include <lwip/netif.h>
#include <lwip/inet.h>
#include <lwip/igmp.h>
#include <lwip/raw.h>
#include <lwip/sockets.h>

#include <mcpwm/mcpwm.h>

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#undef O_RDONLY
#undef O_WRONLY
#undef O_RDWR
#undef O_NONBLOCK
#include <fcntl.h>

#include "../common/toolhelp.h"
#include "../common/net.h"
#include "../common/msgstat.h"
#include "../common/pindef.h"
#include "../common/dimmer.h"
#include "../cmdsvr/cmdsvr.h"

static QueueHandle_t g_msgQ=NULL;
static struct raw_pcb *g_pingPCB=NULL;
static PingRec g_pingRecs[NET_MAX_LEASE_COUNT]={0};
static unsigned int g_pingRecsCount=0;
static unsigned short g_seqNum=0;

static void onTimer(void *arg) {
	Msg msg={
		.id=MSG_CHECK_LEASES
	};
	
	xQueueSend(g_msgQ, &msg, portMAX_DELAY);
}

static void msgTask(void *param) {
	struct netif *nif=NULL;
	Msg msgRecv={0};
	ETSTimer timer={0};
	
	if((nif=sdk_system_get_netif(0))) {
		nif->flags|=NETIF_FLAG_IGMP|NETIF_FLAG_BROADCAST;
		igmp_init();
		igmp_start(nif);
		DBG("IGMP/BROADCAST prepared.\n");
	} else
		DBG("Failed to get net interface.\n");
	
	CMDSVR_init();
	gpio_write(LED_PIN, true);
	
	sdk_os_timer_setfn(&timer, onTimer, NULL);
	sdk_os_timer_arm(&timer, 5000, false);
	
	while(1) {
		xQueueReceive(g_msgQ, &msgRecv, portMAX_DELAY);
		
		switch(msgRecv.id) {
			case MSG_KEY_PRESSED:
				break;
				
			case MSG_CHECK_LEASES: {
				dhcpserver_lease_t leases[NET_MAX_LEASE_COUNT]={0};
				int i;
				
				for(i=0;i<g_pingRecsCount;i++) {
					if(g_pingRecs[i].isAlive)
						break;
				}
				
				if(i==g_pingRecsCount) {
					DBG("No alive client.\n");
				} else
					DBG("Alive client found.\n");
				
				if((g_pingRecsCount=dhcpserver_get_leases(leases, NET_MAX_LEASE_COUNT))) {
					/*
					DBG("hwaddr=%02x:%02x:%02x:%02x:%02x:%02x, ipaddr=%s\n",
					   lease.hwaddr[0], lease.hwaddr[1], lease.hwaddr[2], lease.hwaddr[3], lease.hwaddr[4], lease.hwaddr[5],
					   ip4addr_ntoa(&lease.ipaddr));
					*/
					
					for(i=0;i<g_pingRecsCount;i++) {
						memcpy(&g_pingRecs[i].lease, &leases[i], sizeof(dhcpserver_lease_t));
						g_pingRecs[i].isAlive=false;
						sendEcho(g_pingPCB, &g_pingRecs[i].lease.ipaddr, NET_PING_DATA_SIZE, NET_PING_ID, ++g_seqNum);
					}
				}
				
				sdk_os_timer_arm(&timer, 5000, false);
				break;
			}
		
			default:
				;
		}
	}
}

static u8_t onRecv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr) {
	if(!p || !addr) {
		DBG("Bad argument.\n");
		assert(0);
	}
	
	if(!pbuf_header(p, -PBUF_IP_HLEN)) {
		struct icmp_echo_hdr *iecho=(struct icmp_echo_hdr *)p->payload;

		if(iecho->id==NET_PING_ID &&
			iecho->seqno==htons(g_seqNum)) {
			int i;
			
			DBG("'%s' is alive.\n", ipaddr_ntoa(addr));
			for(i=0;i<g_pingRecsCount;i++) {
				if(!memcmp(&g_pingRecs[i].lease.ipaddr, addr, sizeof(ip_addr_t)))
					g_pingRecs[i].isAlive=true;
			}
			
			pbuf_free(p);
			return 1;
		}
	}

	return 0;
}

static void onGPIO(unsigned char num) {
	static unsigned int prev=0;
	unsigned int now=0;
    
	if(num==KEY_PIN) {
		now=xTaskGetTickCount();
		if(now-prev>=MSEC2TICKS(50)) {
			Msg msg={
				.id=MSG_KEY_PRESSED,
				.param=NULL
			};
			
			prev=now;
			xQueueSendFromISR(g_msgQ, &msg, 0);
		}
	}
}

static void initFS(void) {
    esp_spiffs_init();

    if(esp_spiffs_mount()!=SPIFFS_OK) {
        DBG("Failed to mount SPIFFS.\n");
		assert(false);
    }
}

static void initGPIO(void) {
	gpio_enable(LED_PIN, GPIO_OUTPUT);
	gpio_write(LED_PIN, false);

	gpio_enable(KEY_PIN, GPIO_INPUT);
	gpio_set_pullup(KEY_PIN, true, false);
	GPIO.STATUS_CLEAR=0x0000ffff;
	gpio_set_interrupt(KEY_PIN, GPIO_INTTYPE_EDGE_NEG, onGPIO);
}

static void initDimmer(void) {
	unsigned char pins[]={LED_A_PIN, LED_B_PIN};
	unsigned int max=(1<<DIMMER_RES)-1, a=0, b=0;
	float ar, br;
	
	if(MCPWM_init(DIMMER_FREQ, TIMER_CLKDIV_16, DIMMER_RES, pins, sizeof(pins)/sizeof(pins[0]))<0) {
		DBG("Failed to initialize dimmer.\n");
		assert(false);
	}
	
	DBG("b=%d, c=%d\n", DIMMER_DEF_BRIGHTNESS, DIMMER_DEF_COLORTEMP);
	
	if(!DIMMER_DEF_BRIGHTNESS)
		goto set;
	
	colortempRatio(DIMMER_DEF_COLORTEMP, &ar, &br);
	ar*=(float)DIMMER_DEF_BRIGHTNESS/DIMMER_MAX_BRIGHTNESS;
	br*=(float)DIMMER_DEF_BRIGHTNESS/DIMMER_MAX_BRIGHTNESS;
	a=(unsigned int)((float)DIMMER_CUTOFF+ar*(max-DIMMER_CUTOFF));
	b=(unsigned int)((float)DIMMER_CUTOFF+br*(max-DIMMER_CUTOFF));
	DBG("ar=%f, br=%f, a=%d, b=%d\n", ar, br, a, b);
	
set:
	MCPWM_setMark(0, a);
	MCPWM_setMark(1, b);
}

static void initWiFi(void) {
	spiffs_file fin=SPIFFS_open(&fs, "initParam", SPIFFS_O_RDONLY, 0);
	struct sdk_softap_config apCfg;
	struct ip_info ipAddr;

	sdk_wifi_set_phy_mode(PHY_MODE_11N);
    sdk_wifi_set_opmode(SOFTAP_MODE);

	memset(&apCfg, 0, sizeof(apCfg));
	sdk_wifi_softap_get_config(&apCfg);
	if(fin>=0) {
		int res;
		InitParam init;

		res=SPIFFS_read(&fs, fin, &init, sizeof(init));
		if(res==sizeof(init)) {	
			if(init.fieldMask&0x1) {
				memcpy(apCfg.ssid, init.locSSID, sizeof(init.locSSID));
				DBG("New local SSID=%s.\n", apCfg.ssid);
			}
			if(init.fieldMask&0x2) {
				memcpy(apCfg.password, init.locPassword, sizeof(init.locPassword));
				apCfg.authmode=init.locAuthMode;
				DBG("New local password=%s.\n", apCfg.password);
			}
			apCfg.max_connection=8;
		} else
			DBG("Failed to read initParam. (res=%d)\n", res);
		
		SPIFFS_close(&fs, fin);
		unlink("initParam");
		sdk_wifi_softap_set_config(&apCfg);
	} else
		DBG("Failed to open initParam. (res=%d)\n", fin);

	memset(&ipAddr, 0, sizeof(ipAddr));
	NET_DEF_ADDR(ipAddr.ip);
	NET_DEF_GW(ipAddr.gw);
	NET_DEF_MASK(ipAddr.netmask);
	sdk_wifi_set_ip_info(SOFTAP_IF, &ipAddr);
	
	memset(&ipAddr, 0, sizeof(ipAddr));
	NET_1ST_ADDR(ipAddr.ip);
	dhcpserver_start(&ipAddr.ip, NET_MAX_LEASE_COUNT);
	
	g_pingPCB=raw_new(IP_PROTO_ICMP);
	raw_recv(g_pingPCB, onRecv, NULL);
	raw_bind(g_pingPCB, IP_ADDR_ANY);
}

void user_init(void) {
	initFS();
    uart_set_baud(0, 115200);
	DBG("%s\n", sysStr());
	initGPIO();
	initDimmer();
	initWiFi();

	g_msgQ=xQueueCreate(8, sizeof(Msg));
	xTaskCreate(msgTask, "msgTask", 512, NULL, 4, NULL);
}
