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
#include "../common/pindef.h"
#include "../common/dimmer.h"

#include "wsmsg.h"
#include "ws.h"

unsigned int g_brightness=DIMMER_DEF_BRIGHTNESS;
unsigned int g_colortemp=DIMMER_DEF_COLORTEMP;

static void updatePWM(void) {
	unsigned int max=(1<<DIMMER_RES)-1, a=0, b=0;
	float ar, br;
	
	DBG("b=%d, c=%d\n", g_brightness, g_colortemp);
	
	if(!g_brightness)
		goto set;
	
	colortempRatio(g_colortemp, &ar, &br);
	ar*=(float)g_brightness/DIMMER_MAX_BRIGHTNESS;
	br*=(float)g_brightness/DIMMER_MAX_BRIGHTNESS;
	a=(unsigned int)((float)DIMMER_CUTOFF+ar*(max-DIMMER_CUTOFF));
	b=(unsigned int)((float)DIMMER_CUTOFF+br*(max-DIMMER_CUTOFF));
	DBG("ar=%f, br=%f, a=%d, b=%d\n", ar, br, a, b);
	
set:
	MCPWM_setMark(0, a);
	MCPWM_setMark(1, b);
}

static void setBrightness(unsigned int v) {
	if(v>DIMMER_MAX_BRIGHTNESS)
		v=DIMMER_MAX_BRIGHTNESS;

	g_brightness=v;
	updatePWM();
}

static void setColortemp(unsigned int v) {
	if(v>DIMMER_MAX_COLORTEMP)
		v=DIMMER_MAX_COLORTEMP;
	
	g_colortemp=v;
	updatePWM();
}

void onWSMsg(struct tcp_pcb *pcb, unsigned char *data, unsigned short len, unsigned char mode) {
	char *bufRecv=malloc(len+1);
	unsigned short msgRecv=MSG_MIN;
	const char *arg[16];
	int res=-1;
	const char *bufSend=NULL;
	
	assert(bufRecv);
	memset(bufRecv, 0, len+1);
	memcpy(bufRecv, data, len);
	memset(arg, 0, sizeof(arg));
	if((res=parseRawMsg(bufRecv, &msgRecv, &arg))>=0) {
		switch(msgRecv) {
			case MSG_GET_BRIGHTNESS: {
				DBG("msgRecv=%d\n", msgRecv);
				
				bufSend=makeRawMsg(MSG_GET_BRIGHTNESS_REPLY, "0;%u", g_brightness);

				DBG("bufSend=%s\n", bufSend);
				websocket_write(pcb, (unsigned char *)bufSend, strlen(bufSend), WS_TEXT_MODE);
				break;
			}

			case MSG_SET_BRIGHTNESS: {
				//DBG("msgRecv=%d, arg=%s\n", msgRecv, arg[0]);
				
				setBrightness((unsigned int)atoi(arg[0]));
				
				bufSend=makeRawMsg(MSG_SET_BRIGHTNESS_REPLY, "0");

				//DBG("bufSend=%s\n", bufSend);
				websocket_write(pcb, (unsigned char *)bufSend, strlen(bufSend), WS_TEXT_MODE);
				break;
			}
			
			case MSG_GET_COLORTEMP: {
				DBG("msgRecv=%d\n", msgRecv);
				
				bufSend=makeRawMsg(MSG_GET_COLORTEMP_REPLY, "0;%u", g_colortemp);

				DBG("bufSend=%s\n", bufSend);
				websocket_write(pcb, (unsigned char *)bufSend, strlen(bufSend), WS_TEXT_MODE);
				break;
			}

			case MSG_SET_COLORTEMP: {
				//DBG("msgRecv=%d, arg=%s\n", msgRecv, arg[0]);
				
				setColortemp((unsigned int)atoi(arg[0]));
				
				bufSend=makeRawMsg(MSG_SET_COLORTEMP_REPLY, "0");

				//DBG("bufSend=%s\n", bufSend);
				websocket_write(pcb, (unsigned char *)bufSend, strlen(bufSend), WS_TEXT_MODE);
				break;
			}
			
			case MSG_KEEPALIVE: {
				DBG("msgRecv=%d\n", msgRecv);
				
				bufSend=makeRawMsg(MSG_KEEPALIVE_REPLY, "0");

				DBG("bufSend=%s\n", bufSend);
				websocket_write(pcb, (unsigned char *)bufSend, strlen(bufSend), WS_TEXT_MODE);
				break;
			}			

			default:
				;
		}
	}

	free(bufRecv);
}

void onWSOpen(struct tcp_pcb *pcb, const char *uri) {
    if(strcmp(uri, "/CmdSvr.ws")) {
		websocket_write(pcb, NULL, 0, 0x08);
		DBG("Illegal URL detected. Disconnect immediately.\n");
	}
}

void initWS(void) {
	websocket_register_callbacks((tWsOpenHandler)onWSOpen, (tWsHandler)onWSMsg);
}
