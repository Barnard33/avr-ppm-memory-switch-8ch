/*
 *
 * avr-ppm-memory-switch-8ch.c
 *
 * Copyright (C) 2022  Barnard33
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * This code is only intended to be used for educational purposes.
 * It is not production stable and must under no circumstance be used 
 * in any kind of radio controlled machinery, e.g., planes, cars, boats, etc.
 *
 * Created: 2022-02-23 22:22:22 
 *
 */

#ifndef F_CPU
#define F_CPU 1000000
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

typedef enum {
    FALSE,
    TRUE
} boolean_t;

typedef enum {
    OFF, 
    ON
} gpio_state_t;

typedef enum {
    UNDEFINED,
    BACKWARD,
    NEUTRAL,
    FORWARD
} direction_t;

typedef struct {
    volatile uint8_t * port;
    uint8_t pin;
} gpio_t;

/* global constants */

#define SWITCHABLE_GPIOS_SIZE 7
const gpio_t switchable_gpios[SWITCHABLE_GPIOS_SIZE] = {
    {&PORTA, PA7}, // PWM 8 bit timer0
    //{&PORTA, PA6}, // PWM 16 bit timer1
    {&PORTA, PA3},
    {&PORTA, PA2},
    {&PORTA, PA1},
    {&PORTA, PA0},
    {&PORTB, PB0},
    {&PORTB, PB1},
};

/* global variables */
volatile boolean_t ppm_signal_received = FALSE;
volatile uint8_t ppm_pulse_length = 0;

inline static void init_interrupts(void) {
	/* activate INT0 on PB2 */
	GIMSK |= (1 << INT0);
	/* generate interrupt on rising edge for INT0 */
	MCUCR |= ((1 << ISC01) | (1 << ISC00));
}

inline static int8_t is_int0_on_rising_edge(void) {
    return (MCUCR & (1 << ISC00)) > 0;
}

inline static void set_int0_on_rising_edge(void) {
    MCUCR |= (1 << ISC00);
}

inline static void set_int0_on_falling_edge(void) {
    MCUCR &= ~(1 << ISC00);
}

inline static void connect_ppm_in(void) {
    /* configure pin PB2/INT0 as input for PPM in */
    DDRB &= ~(1 << DDB2);
}

inline static void init_timer0_ppm_in(void) {
    /* configure timer0 to use prescaler with clkio/64 */
    TCCR0B |= (1 << CS00) | (1 << CS01);
}

inline static void init_timer0_pwm(void) {
	/* clear OC0B on compare match when up-counting
	   set OC0B on compare match when down-counting */
	TCCR0A |= (1 << COM0B1);
	
	/* phase correct PWM, 8-bit */
	TCCR0A |= (1 << WGM00);
    TCCR0B |= (1 << WGM02);
	
	/* set clock to clkIO (no prescaler) */
	TCCR0B |= (1 << CS00);
}

inline static void set_timer0_pwm_duty_cycle(uint8_t value) {
    OCR0B = value;
}

inline static void set_timer0_pwm_pulse_rate(uint8_t value) {
    OCR0A = value;
}

inline static void init_timer1_pwm(void) {
	/* clear OC1A on compare match when up-counting
	   set OC1A on compare match when down-counting */
	TCCR1A |= (1 << COM1A1);
	
    /* phase and frequency correct PWM
       TOP is ICR1 */
    //TCCR1A |= (1 << WGM10);
    TCCR1B |= (1 << WGM13);

	/* set clock to clkIO (no prescaler) */
	TCCR1B |= (1 << CS10);
}

inline static void set_timer1_pwm_duty_cycle(uint16_t value) {
    OCR1A = value;
}

inline static void set_timer1_pwm_pulse_rate(uint16_t value) {
    ICR1 = value;
}

inline static void connect_gpios(void) {
    DDRA |= (1 << DDA7); //GPIO_0, OC0B PWM timer 0
    DDRA |= (1 << DDA6); //GPIO_1, OC1A PWM timer 1
    DDRA |= (1 << DDA3); //GPIO_2
    DDRA |= (1 << DDA2); //GPIO_3
    DDRA |= (1 << DDA1); //GPIO_4
    DDRA |= (1 << DDA0); //GPIO_5
    DDRB |= (1 << DDB0); //GPIO_6
    DDRB |= (1 << DDB1); //GPIO_7
}

inline static void set_gpio_state(gpio_t gpio, gpio_state_t state) {
    if(state == ON) {
        (*gpio.port) |= (1 << gpio.pin); 
    }
    else {
        (*gpio.port) &= ~(1 << gpio.pin);
    } 
}

inline static void toggle_gpio_state(gpio_t gpio) {
    (*gpio.port) ^= (1 << gpio.pin); 
}

inline static direction_t get_direction(uint8_t pulse_length) {
    if(pulse_length > 27) {
        return FORWARD;
    }
    else if(pulse_length < 26 && pulse_length > 20) {
        return NEUTRAL;
    }
    else if(pulse_length < 19) {
        return BACKWARD;
    }
    else {
        return UNDEFINED;
    }
}

int main(void) {
    init_interrupts();
    connect_ppm_in();

    init_timer0_ppm_in();

    //init_timer0_pwm();
    //set_timer0_pwm_pulse_rate(0xFF);

    init_timer1_pwm();
    set_timer1_pwm_pulse_rate(60);

    connect_gpios();

    _delay_ms(2000);

    //start with a higher duty cycle then slow down
    //set_timer0_pwm_duty_cycle(25);  
    set_timer1_pwm_duty_cycle(50);
    _delay_ms(500);
    //set_timer0_pwm_duty_cycle(11);
    set_timer1_pwm_duty_cycle(3);

    sei();
    
    uint8_t counter = 0;
    direction_t last = UNDEFINED;

    while(1) {
        while(!ppm_signal_received) {
            _delay_ms(1);
        }
        ppm_signal_received = FALSE;

        direction_t current = get_direction(ppm_pulse_length);

        /*
         * works as follows: 
         * - moving joystick from middle to forward selects next switchable GPIO pin
         * - moving joystick from middle to backward toggles the selected GPIO pin and resets switch selection
         * - moving joystick from middle to backward without previous switch selection turns all switches off
         * - if switch selection is higher than number of switchable GPIO pins, simply nothing happens
         */
        if(last == NEUTRAL && current == FORWARD) {
            if(counter <= SWITCHABLE_GPIOS_SIZE) {
                counter++;
            }
        } 
        else if(last == NEUTRAL && current == BACKWARD) {
            if(counter > 0 && counter <= SWITCHABLE_GPIOS_SIZE) {
                toggle_gpio_state(switchable_gpios[counter - 1]);
            }
            if(counter == 0) {
                for(uint8_t i = 0; i < SWITCHABLE_GPIOS_SIZE; i++) {
                    set_gpio_state(switchable_gpios[i], OFF);
                }
            }
            counter = 0;
        }

        if(current != UNDEFINED) {
            last = current;
        }
    }
}

/* servo input pin on INT0 */
ISR(INT0_vect) {
	if(is_int0_on_rising_edge()) {
		TCNT0 = 0;
		set_int0_on_falling_edge();
	}
	else {
        ppm_pulse_length = TCNT0;
		ppm_signal_received = TRUE;
        set_int0_on_rising_edge();
	}
}
