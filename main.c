//ATtiny85-based PID temperature controller
//
//
//connection:
//
//
//
//              |----------------------|
//              |                      |
//              |                      |
//              |                      |
//              |                      |
//              |                      |
//              |                      |
//              |                      |
//              |                      |
//              |                      |
//              |    ATtiny25/45/85    |
//              |                      |
//              |           [ADC_TEMP] |<-------------thermal couple-------------|
//              |                      |                                         |
//              |                      |                                         |
//              |                      |                                         |
//              |                      |                         |----|------<[Heater]-----<Power
//              |             OC1A/PB1 |--------> SW+ >----------| SW |
//              |                      |                         |----|-------|GND
//              |                      |
//              |            _OC1A/PB0 |--------> SW- (out of phase with SW+)
//              |                      |
//              |----------------------|
//
//
//
//Note: code also supports LM35 if you simply replace the T2ADC() with the one for LM35.
//      support for LM36 is possible if the ADC is configured to use 5v Vref
//
//
//

#include "gpio.h"
#include "delay.h"							//we use software delays
#include "adc.h"							//we use adc
#include "pwm1.h"
#include "iPID.h"

//hardware configuration
//#define USE_PID								//comment out if PID control is not used. In this case, simple on/off control is used

#define SW_PORT			PORTB
#define SW_DDR			DDRB
#define SW_PIN			(1<<1)				//pin/pins to switch on off the heater, Active high

#define TEMPtgt			55					//target temperature, in C
#define TEMP_DLY		10					//control loop interval, in ms. Use 1000ms or bigger in actual implementation

//#define ADC_IN			ADC_TEMP			//to use with internal temperature sensor
#define ADC_IN			ADC_ADC1			//AIN1 -> to be replaced with the internal diode sensor in production
//end hardware configuration

//global defines
//ATtiny85 Temperature sensor curve - see datasheet 17.12
#define T2ADC(T)		(230 + (int32_t) (370-230) * ((T) + 40) / (85 + 40))
//#define T2ADC(T)		(((unsigned long) (T) << 10) / 110)		//for use with LM35 temperature sensor

//Heater control pin for simple on/off control. Active high
#define HEATER_ON()		IO_SET(SW_PORT, SW_PIN)
#define HEATER_OFF()	IO_CLR(SW_PORT, SW_PIN)

//global variables
#if defined(USE_PID)
PID_t pid0;									//PID controller
volatile int32_t control;					//control variable
uint8_t dc;									//duty cycle
#endif

int main(void) {
	uint16_t temp;							//adc results for analog input
	int8_t noise;							//random noise

	mcu_init();								//reset the mcu
	adc_init();								//reset the adc -> 1.1v reference
#if defined(USE_PID)
	PID_init(&pid0, 20, 1, 1);
	PID_set(&pid0, T2ADC(TEMPtgt));
	pwm1_init(TMR1_PS64x);
	pwm1a_setdc(10);						//output on pwm1a
#else
	IO_OUT(SW_DDR, SW_PIN);					//output pin for simple on/off control
#endif
	while(1) {
		temp = adc_read(ADC_IN);			//sample the temperature
#if defined(USE_PID)
		//control = T2ADC(TEMPtgt) - temp;
		//if (control < 0) dc = (dc==  2)?2:(dc-1);
		//if (control > 0) dc = (dc==250)?250:(dc+1);
		control = PID(&pid0, temp);
		pid0.control /= 4;					//reduce overall gain
		dc = PID_limit(&pid0, 2, 253);		//limit duty cycle to 2/255 to 253/255
		pwm1a_setdc(dc);					//set duty cycle
#else
		//ON / off control with simple hysteresis
		if (temp > T2ADC(TEMPtgt)+10) HEATER_OFF();
		if (temp < T2ADC(TEMPtgt)-10) HEATER_ON();
#endif
		delay_ms(TEMP_DLY);						//slow down control steps
	}

	return 0;
}
