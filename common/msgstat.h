#ifndef MSGSTAT_H
#define MSGSTAT_H

#ifdef __cplusplus
extern "C" {
#endif

#define MSG_MIN					(0)

#define STAT_MIN				(0)
#define STAT_IDLE				(STAT_MIN+1)

typedef struct __attribute__((packed)) {
	unsigned int id;
	void *param;
} Msg;

#ifdef __cplusplus
}
#endif

#endif
