#ifndef WSMSG_H
#define WSMSG_H

#ifdef __cplusplus
extern "C" {
#endif

#define MSG_MIN						(0)
#define MSG_SET_BRIGHTNESS			(MSG_MIN+1)
#define MSG_GET_BRIGHTNESS			(MSG_MIN+2)
#define MSG_SET_COLORTEMP			(MSG_MIN+3)
#define MSG_GET_COLORTEMP			(MSG_MIN+4)
#define MSG_KEEPALIVE				(MSG_MIN+5)
#define MSG_SET_BRIGHTNESS_REPLY	(MSG_MIN+6)
#define MSG_GET_BRIGHTNESS_REPLY	(MSG_MIN+7)
#define MSG_SET_COLORTEMP_REPLY		(MSG_MIN+8)
#define MSG_GET_COLORTEMP_REPLY		(MSG_MIN+9)
#define MSG_KEEPALIVE_REPLY			(MSG_MIN+10)

int parseRawMsg(char *buf, unsigned short *msg, const char * (*arg)[16]);
const char *makeRawMsg(unsigned short msg, const char *fmt, ...);
			
#ifdef __cplusplus
}
#endif

#endif
