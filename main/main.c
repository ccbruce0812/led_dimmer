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
#include <lwip/ip4_addr.h>
#include <lwip/ip_addr.h>
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

#include "../common/private_ssid_config.h"
#include "../common/toolhelp.h"
#include "../common/msgstat.h"
#include "../common/pindef.h"
#include "../common/dimmer.h"
#include "../cmdsvr/cmdsvr.h"

QueueHandle_t g_msgQ=NULL;
unsigned char g_curStat=STAT_IDLE;

static void onTimer(void *arg) {
	Msg msg={
		.id=MSG_CHECK_LEASES
	};
	
	xQueueSend(g_msgQ, &msg, portMAX_DELAY);
}

static void onCheckLeases(void) {
	dhcpserver_lease_t lease;
	
	if(dhcpserver_get_leases(&lease, 1)) {
		DBG("hwaddr=%02x:%02x:%02x:%02x:%02x:%02x, ipaddr=%s\n",
		   lease.hwaddr[0], lease.hwaddr[1], lease.hwaddr[2], lease.hwaddr[3], lease.hwaddr[4], lease.hwaddr[5],
		   ip4addr_ntoa(&lease.ipaddr));
	}
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
	sdk_os_timer_setfn(&timer, onTimer, NULL);
	sdk_os_timer_arm(&timer, 5000, false);
	gpio_write(LED_PIN, true);
	
	while(1) {
		xQueueReceive(g_msgQ, &msgRecv, portMAX_DELAY);
		
		switch(msgRecv.id) {
			case MSG_KEY_PRESSED:
				break;
				
			case MSG_CHECK_LEASES:
				onCheckLeases();
				sdk_os_timer_arm(&timer, 5000, false);
				break;
		
			default:
				;
		}
	}
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
	IP4_ADDR(&ipAddr.ip, 192, 168, 254, 254);
	IP4_ADDR(&ipAddr.gw, 192, 168, 254, 254);
	IP4_ADDR(&ipAddr.netmask, 255, 255, 255, 0);
	sdk_wifi_set_ip_info(SOFTAP_IF, &ipAddr);
	
	memset(&ipAddr, 0, sizeof(ipAddr));
	IP4_ADDR(&ipAddr.ip, 192, 168, 254, 100);
	dhcpserver_start(&ipAddr.ip, 10);
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
