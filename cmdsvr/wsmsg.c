#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "../common/toolhelp.h"

#include "wsmsg.h"

typedef struct {
	const char *str;
	unsigned short id;
} MsgMapItem;

static const MsgMapItem g_msgMap[]={
	{"MSG_SET_BRIGHTNESS",			MSG_SET_BRIGHTNESS},	
	{"MSG_GET_BRIGHTNESS",			MSG_GET_BRIGHTNESS},		
	{"MSG_SET_COLORTEMP",			MSG_SET_COLORTEMP},	
	{"MSG_GET_COLORTEMP",			MSG_GET_COLORTEMP},
	{"MSG_KEEPALIVE",				MSG_KEEPALIVE},
	{"MSG_SET_BRIGHTNESS_REPLY",	MSG_SET_BRIGHTNESS_REPLY},
	{"MSG_GET_BRIGHTNESS_REPLY",	MSG_GET_BRIGHTNESS_REPLY},
	{"MSG_SET_COLORTEMP_REPLY",		MSG_SET_COLORTEMP_REPLY},	
	{"MSG_GET_COLORTEMP_REPLY",		MSG_GET_COLORTEMP_REPLY},
	{"MSG_KEEPALIVE_REPLY",			MSG_KEEPALIVE_REPLY},
	{NULL,							MSG_MIN}
};

static unsigned short str2Msg(const char *str) {
	int i;
	
	for(i=0;g_msgMap[i].str;i++) {
		if(!strcmp(g_msgMap[i].str, str))
			return g_msgMap[i].id;
	}
	
	return MSG_MIN;
}

static const char *msg2Str(unsigned short id) {
	int i;
	
	for(i=0;g_msgMap[i].str;i++) {
		if(g_msgMap[i].id==id)
			return g_msgMap[i].str;
	}
	
	return NULL;
}

int parseRawMsg(char *buf, unsigned short *msg, const char * (*arg)[16]) {
	if(!buf || !msg || !arg)
		return -1;
	
	int i=0;
	char *now=NULL;
	
	now=strtok(buf, ",");
	if(!now || (*msg=str2Msg(now))==MSG_MIN)
		return -1;
	
	while(i<16 && (now=strtok(NULL, ";")))
		(*arg)[i++]=now;
	
	return i;
}

const char *makeRawMsg(unsigned short msg, const char *fmt, ...) {
	static char rawMsg[64];
	char body[64];
	const char *str=msg2Str(msg);
	va_list arg;
	
	if(!str)
		return NULL;
	
	if(fmt) {
		sprintf(body, "%s,%s", str, fmt);
		va_start(arg, fmt);
		vsprintf(rawMsg, body, arg);
		va_end(arg);
	} else
		strcpy(rawMsg, str);
	
	return rawMsg;
}
