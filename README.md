# avr-ppm-memory-switch-8ch

The eight channel memory switch works with one channel of a PPM RC system.

- Moving the joystick from middle to forward selects the next switchable GPIO pin
- Moving the joystick from middle to backward toggles the selected GPIO pin and resets switch selection
- Moving the joystick from middle to backward without previous switch selection turns all switches off
- If the selected switch is higher than number of switchable GPIO pins, simply nothing happens

One GPIO pin is reserved for driving a gear motor with PWM, e.g. for a radar antenna.

The program runs on an ATtiny24 MCU.

Ouput pin configuration:

- GPIO_0: PA7, switch 1
- GPIO_1: OC1A (PA6), PWM on 16 bit timer 1
- GPIO_2: PA3, switch 2
- GPIO_3: PA2, switch 3
- GPIO_4: PA1, switch 4
- GPIO_5: PA0, switch 5
- GPIO_6: PB0, switch 6
- GPIO_7: PB1, switch 7

Input pin configuration:

- PPM_IN: INT0 (PB2)
