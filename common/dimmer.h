#ifndef DIMMER_H
#define DIMMER_H

#ifdef __cplusplus
extern "C" {
#endif

#define DIMMER_FREQ				(10000)
#define DIMMER_DIV				(TIMER_CLKDIV_16)
#define DIMMER_RES				(4)
#define DIMMER_DEF_BRIGHTNESS	(((1<<DIMMER_RES)-1)/2)
#define DIMMER_DEF_COLORTEMP	(((1<<DIMMER_RES)-1)/2)

#ifdef __cplusplus
}
#endif

#endif
