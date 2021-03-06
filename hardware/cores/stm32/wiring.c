/*
  wiring.c - Partial implementation of the Wiring API for the STM32Fxxxx.
  Part of Arduino - http://www.arduino.cc/

  Copyright (c) 2005-2006 David A. Mellis
  Copyright (c) 2009 Magnus Lundin

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General
  Public License along with this library; if not, write to the
  Free Software Foundation, Inc., 59 Temple Place, Suite 330,
  Boston, MA  02111-1307  USA

  $Id: wiring.c 367 2007-12-14 04:21:59Z mellis $
*/

/*
	JTAG Pins should be converted to normal GPIO, this can be overridden with specific input pin values on boot.
	AFIO_MAPR |= (4<<24)
	
	The Systick Interrupt must be enabled for mills() and micros() to work
*/

#include "wiring_private.h"

/* Disable interrupts */
#define DisableInterrups()	__asm("    cpsid	i\n")
/* Enable interrupts */
#define EnableInterrups()	__asm("    cpsie	i\n")


// PLL frequency
#define MCK 72000000UL
// Use Cortex-M3 systick for basic timekeeping.
// Systick is configured for 1kHz
#define TCK	1000

// Must be volatile or gcc will optimize away some uses of it.
volatile unsigned long systick_count;
volatile unsigned timeout=0;

void SysTickHandler(void)
{
	systick_count++;
	if (timeout) timeout--;
}

unsigned long millis()
{
	return systick_count;
}

unsigned long micros()
{
	// Glitch free clock
	long v0 = SysTick->VAL;
	long c0 = systick_count;
	long v1 = SysTick->VAL;
	long c1 = systick_count;
	
	if (v1 < v0) 
		// Downcounting, no systick rollover
		return c0*8000-v1/(MCK/8000000UL);
	else
		// systick rollover, use last count value
		return c1*8000-v1/(MCK/8000000UL);
}

void delay(unsigned long ms)
{
	unsigned long start = millis();
	
	while (millis() - start < ms)
		;
}

/* Delay for the given number of microseconds.  Assumes a 72 MHz clock. 
 * Disables interrupts, which will disrupt the millis() function if used
 * too frequently. */
void delayMicroseconds(unsigned int us)
{
	uint32_t startUs = micros();
	uint32_t endUs = startUs + 8*us;

//	DisableInterrups();
	
	if (endUs>startUs)
		while (micros() < endUs )
			;
	else
	{
		// Handle micros() overflow
		while (micros() >= startUs)
			;
		while (micros() < endUs)
			;
	}

//	EnableInterrups();
}


#define ADC_CHANNEL_COUNT 1

void init()
{
	// this needs to be called before setup() or some functions won't
	// work there
	
	// Configure system clocks
	SystemInit ();

	// Enable GPIO port clocks
    RCC->APB2ENR = RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_AFIOEN;
	// Enable timer clocks
    RCC->APB1ENR = RCC_APB1ENR_TIM2EN | RCC_APB1ENR_TIM4EN;
    // Pin configurations and remappings
    /* Disable JTAG functionality */
    AFIO->MAPR = (AFIO->MAPR&~(7<<24))|(4<<24);
	
	// systick is used for millis() and delay()
	SysTick->LOAD = (MCK/TCK)-1;
	/* Enable SysTick and TICKINT using MCK as clock source */
	SysTick->CTRL = 7; /* NVIC_ST_CTRL_CLK_SRC | NVIC_ST_CTRL_INTEN | NVIC_ST_CTRL_ENABLE */ 
	systick_count = 0;


	// Configure timers
	// For direct servo control we need a period of 25ms, 2MHz with 16 bit timers works well
	
	// For interrupt driven servo control we can run 8 servos from every timer

	// timers for phase-correct hardware dc motor control, we can run PWM at 10kHz with a timer at 36MHz for very precise control

	// set adc prescale factor

	// enable a2d conversions

	/* Start peripherial clock and reset peripherial */
	RCC->APB2ENR = RCC_APB2ENR_ADC1EN; /* ADC1EN */ /* bit 9 */
	RCC->APB2RSTR |= RCC_APB2RSTR_ADC1RST; /* ADC1RST */ /* bit 9 */
	RCC->APB2RSTR &= ~RCC_APB2RSTR_ADC1RST; /* ADC1RST */ /* bit 9 */

	/* Configure ADC */
	/* One channel single conversion mode, no DMA and no IRQ */
	ADC1->CR1 = 0;  /* Discontinous mode enable */
	ADC1->CR2 = 0; 
	ADC1->SMPR1 = (ADC_SampleTime_28Cycles5<<21)|(ADC_SampleTime_28Cycles5<<18); /* Sample time is 1.5 ADC clocks */ 
	ADC1->SMPR2 = 0; 
	ADC1->SQR1 = ((ADC_CHANNEL_COUNT-1)<<20); /* 1 regular conversion */
	ADC1->SQR2 = 0;
	ADC1->SQR3 = 0;

	
	/* Enable ADC */
	analogEnable(true);

	analogCalibrate();

	EnableInterrups();
}
