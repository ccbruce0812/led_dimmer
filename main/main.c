#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include <esp8266.h>
#include <esp/uart.h>

#include <etstimer.h>
#include <espressif/esp_common.h>

#include <dhcpserver.h>
#include <esp_spiffs.h>

#include <lwip/netif.h>
#include <ipv4/lwip/inet.h>
#include <ipv4/lwip/igmp.h>
#include <lwip/sockets.h>

#include <mcpwm/mcpwm.h>

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
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

static void msgTask(void *param) {
	struct netif *nif=NULL;
	Msg msgRecv={0};
	
	if((nif=sdk_system_get_netif(0))) {
		nif->flags|=NETIF_FLAG_IGMP|NETIF_FLAG_BROADCAST;
		igmp_init();
		igmp_start(nif);
		DBG("IGMP/BROADCAST prepared.\n");
	} else
		DBG("Failed to get net interface.\n");
	
	CMDSVR_init();
	
	while(1) {
		xQueueReceive(g_msgQ, &msgRecv, portMAX_DELAY);
		
		switch(msgRecv.id) {
			default:
				;
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

static void initDimmer(void) {
	unsigned char pins[]={LED_A_PIN, LED_B_PIN};
	
	if(MCPWM_init(DIMMER_FREQ, TIMER_CLKDIV_16, DIMMER_RES, pins, sizeof(pins)/sizeof(pins[0]))<0) {
		DBG("Failed to initialize dimmer.\n");
		assert(false);
	}
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
	initDimmer();
	initWiFi();

	g_msgQ=xQueueCreate(8, sizeof(Msg));
	xTaskCreate(msgTask, "msgTask", 512, NULL, 4, NULL);
}
