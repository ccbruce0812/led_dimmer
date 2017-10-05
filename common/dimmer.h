#ifndef DIMMER_H
#define DIMMER_H

#ifdef __cplusplus
extern "C" {
#endif

#define DIMMER_FREQ				(1000)
#define DIMMER_DIV				(TIMER_CLKDIV_16)
#define DIMMER_RES				(8)
#define DIMMER_CUTOFF			(50)

#define DIMMER_MAX_BRIGHTNESS	(100)
#define DIMMER_MAX_COLORTEMP	(8)
#define DIMMER_DEF_BRIGHTNESS	(50)
#define DIMMER_DEF_COLORTEMP	(4)

static inline void colortempRatio(unsigned int v, float *a, float *b) {
	if(v>DIMMER_MAX_COLORTEMP)
		v=DIMMER_MAX_COLORTEMP;
	
	*a=(v+1)/10.0;
	*b=1.0-*a;
}

#ifdef __cplusplus
}
#endif

#endif
