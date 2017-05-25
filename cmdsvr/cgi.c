#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include <esp8266.h>
#include <esp/uart.h>

#include <etstimer.h>
#include <espressif/esp_common.h>
#include <espressif/esp_system.h>

#include <esp_spiffs.h>

#include <lwip/inet.h>

#include <mcpwm/mcpwm.h>
#include <httpd/httpd.h>

#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#include "../common/private_ssid_config.h"
#include "../common/toolhelp.h"

#include "cgi.h"

static char *onCGI(int idx, int count, char *param[], char *value[]);

const tCGI g_cgiTab[]={
	{"/cmdSvr.cgi", (tCGIHandler)onCGI}
};

ETSTimer g_timer={0};

static void onDeepSleep(void *param) {
	sdk_system_deep_sleep(0);
}

static void onRestart(void *param) {
	sdk_system_restart();
}

static const char *readValue(int count, char *param[], char *value[], const char *key) {
	int i=0;
	
	for(i=0;i<count;i++) {
		if(!strcmp(param[i], key))
			return value[i];
	}
	
	return NULL;
}

static char *onCGI(int idx, int count, char *param[], char *value[]) {
	const char *val=NULL;
	char *ret="/aborted.html";
	
	if((val=readValue(count, param, value, "action"))) {
		switch(atoi(val)) {
			case 0: {
				InitParam init;
				struct sdk_softap_config apCfg;
				spiffs_file fout;
				
				memset(&init, 0, sizeof(init));
				memset(&apCfg, 0, sizeof(apCfg));
				sdk_wifi_softap_get_config(&apCfg);
				if((val=readValue(count, param, value, "lcSSID")) && strlen(val) && strcmp(val, (const char *)apCfg.ssid)) {
					init.fieldMask|=0x1;
					strncpy((char *)init.locSSID, val, 32);
				}
				if((val=readValue(count, param, value, "lcPass")) && strlen(val)) {
					init.fieldMask|=0x2;
					strncpy((char *)init.locPassword, val, 64);
					init.locAuthMode=AUTH_WPA2_PSK;
				}
				sdk_wifi_softap_set_config(&apCfg);

				fout=SPIFFS_open(&fs, "initParam", SPIFFS_O_WRONLY|SPIFFS_O_CREAT, 0);
				if(fout>=0) {
					int res;
					
					res=SPIFFS_write(&fs, fout, &init, sizeof(init));
					if(res==sizeof(init)) {
						if(init.fieldMask&0x1)
							DBG("New local SSID=%s.\n", init.locSSID);
						if(init.fieldMask&0x2)
							DBG("New local password=%s.\n", init.locPassword);
					} else
						DBG("Failed to write initParam. (res=%d)\n", res);
					SPIFFS_close(&fs, fout);
				} else
					DBG("Failed to open initParam. (res=%d)\n", fout);

				ret="/setWiFi.ssi";
				goto end;
			}
			
			case 1: {
				sdk_ets_timer_setfn(&g_timer, onDeepSleep, NULL);
				sdk_ets_timer_arm(&g_timer, 5000, false);

				ret="/shutdown.html";
				break;
			}
			
			case 2: {
				sdk_ets_timer_setfn(&g_timer, onRestart, NULL);
				sdk_ets_timer_arm(&g_timer, 5000, false);

				ret="/restart.html";
				break;
			}
			
			default:
				;
		}
	}

end:
    return ret;
}

void initCGI(void) {
	http_set_cgi_handlers(g_cgiTab, sizeof(g_cgiTab)/sizeof(g_cgiTab[0]));
}
