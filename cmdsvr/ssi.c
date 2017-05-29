#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include <esp8266.h>
#include <esp/uart.h>

#include <etstimer.h>
#include <espressif/esp_common.h>

#include <lwip/inet.h>

#include <mcpwm/mcpwm.h>
#include <httpd/httpd.h>

#include <string.h>
#include <assert.h>

#include "../common/toolhelp.h"
#include "../common/dimmer.h"

#include "ssi.h"

const char *g_ssiTab[]={
	"version",
	"lcSSID",
	"maxBri",
	"maxCol",
	"defBri",
	"defCol"
};

int onSSI(int idx, char *ins, int len) {
    switch(idx) {
        case 0:
            snprintf(ins, len, "%s", sysStr());
            break;
			
		case 1: {
			struct sdk_softap_config cfg;
			
			memset(&cfg, 0, sizeof(cfg));
			sdk_wifi_softap_get_config(&cfg);
			snprintf(ins, len, "%s", cfg.ssid);
			break;
		}
		
		case 2:
			snprintf(ins, len, "%d", (1<<DIMMER_RES)-1);
			break;
		
		case 3:
			snprintf(ins, len, "%d", (1<<DIMMER_RES)-1);
			break;
			
		case 4:
			snprintf(ins, len, "%d", DIMMER_DEF_BRIGHTNESS);
			break;
		
		case 5:
			snprintf(ins, len, "%d", DIMMER_DEF_COLORTEMP);
			break;

        default:
            snprintf(ins, len, "N/A");
    }

    return (strlen(ins));
}

void initSSI(void) {
	http_set_ssi_handler((tSSIHandler)onSSI, g_ssiTab, sizeof(g_ssiTab)/sizeof(g_ssiTab[0]));
}
