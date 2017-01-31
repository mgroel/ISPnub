/**
 * @mainpage
 * ISPnub firmware project
 *
 * @section description Description
 * ISPnub is a stand-alone AVR programming module. The programming instructions
 * are defined within scripts. These scripts are converted with an additional
 * tool (ISPnubCreator) into binary information and are stored in flash. This
 * firmware interprets this binary programming instructions.
 * 
 * The firmware hex file is packed into the JAR file of ISPnubCreator which
 * merges the firmware hex data with programming instructions from scripts.
 * 
 * Environment:
 * - Target: ATmega1284P
 * - Compiler: avr-gcc (GCC) 4.7.2
 * 
 * @section history Change History
 * - v1.0 (2013-06-14)
 *   - Initial release
 * - v1.1 (2013-10-06)
 *   - Code cleanup and documentation
 * - v1.2 (2014-04-08)
 *   - Added EEPROM programming
 * - v1.3 (2017-01-29)
 *   - Improvements for Battery-Powered Devices (TODO)
 *   - State-Machine in Main (TODO)
 *   - Buzzer on PBxxx (TODO)
 *   - made slowticker volatile
 *   - 
 *
 */

/**
 * @file main.c
 *
 * @brief This file contains the main routine with application entry point for
 *        the ISPnub firmware project
 *
 * @author Thomas Fischl
 * @copyright (c) 2013-2014 Thomas Fischl
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#include <inttypes.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h> 
#include "clock.h"
#include "isp.h"
#include "counter.h"
#include "hal.h"
#include "script.h"
#include "debounce.h"


//states
#define S_INIT 0
#define S_IDLE 10
#define S_PROGRAMMING 20
#define S_PROGRAMMING_SUCCESS 21
#define S_PROGRAMMING_FAILED 22
#define S_SLEEP 30
#define S_WAKEUP 31
#define S_NO_MORE 40	//counter empty
#define S_NO_PROGRAM 41



/**
 * @brief Main routine of firmware and application entry point
 * @return Application return code
 */
int main(void) {

    hal_init();
	hal_enableINT0();
	hal_enableINT1();
    clock_init();
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	
	
    uint8_t ticker10MS = clock_getTickerFast();
    uint8_t ticker250MS = clock_getTickerSlow();
	uint8_t sleeptimer = clock_getTickerSlow();
	
    uint8_t success = 1;
    
	uint8_t buzzer = 0;		//time to turn on in multiple by 10ms
	uint8_t toggle = 0;
	
	uint8_t state = S_INIT;
	
    // enable interrupts
    sei();
	
	
    // main loop	
    while (1) {
		
		
		//slow ticks
		if (clock_getTickerSlowDiff(ticker250MS) > CLOCK_TICKER_SLOW_250MS) {
            ticker250MS = clock_getTickerSlow();
			
            toggle = !toggle;
		}
		
		//fast ticks
		if (clock_getTickerSlowDiff(ticker10MS) > CLOCK_TICKER_FAST_10MS) {
            ticker10MS = clock_getTickerSlow();
			
			
			tickDebounce();
			
			if(buzzer!=0)
				buzzer--;
		}
		
		//buzzer
		if (buzzer==0) {
			hal_setBuzzer(0);
		} else {
			hal_setBuzzer(1);
		}
		//led signaling
		switch(state) {
			case S_INIT:
			case S_WAKEUP:
				hal_setLEDgreen(1);
				hal_setLEDred(0);
				hal_setBuzzer(0);
				
				
				//check whether there is a valid program or just the dummy
				/*if(scriptdata[0]==SCRIPT_CMD_END) { //TODO
					state=S_NO_PROGRAM;
				} */
				
				
				break;
				
			case S_IDLE:
				if(success) {
					hal_setLEDgreen(1);
					hal_setLEDred(0);
				} else {
					hal_setLEDgreen(0);
					hal_setLEDred(toggle);
				}
				break;
			
			case S_PROGRAMMING:
				hal_setLEDgreen(1);
				hal_setLEDred(1);
				
				break;
			case S_PROGRAMMING_SUCCESS:

				break;
			case S_PROGRAMMING_FAILED:
				
				break;
			case S_NO_MORE:
				hal_setLEDgreen(toggle);
				hal_setLEDred(toggle);
				
				break;
				
			case S_NO_PROGRAM:
				hal_setLEDgreen(!toggle);
				hal_setLEDred(toggle);
				
				break;
			case S_SLEEP:
				hal_setLEDgreen(0);
				hal_setLEDred(0);
				hal_setBuzzer(0);
				break;
		}
		
		
		
		//processing
		switch(state) {
			case S_INIT:
			case S_WAKEUP:
				
				//remaining cycles to program?
				if(counter_read()==0) {
					state=S_NO_MORE;
				}
				
				
				state=S_IDLE;
				break;
			
			case S_IDLE:
				
				
				if( get_key_press( (1 << IO_SWITCH ) | (1 << IO_EXT_SWITCH) ) ) {
					sleeptimer=clock_getTickerSlow();
					if(counter_read()>0) {
						state=S_PROGRAMMING;
					} else {
						state=S_NO_MORE;
					}
				}
				
				//go to sleep?
				cli();	//for atomic check of condition
				if (clock_getTickerSlowDiff(sleeptimer) < CLOCK_TICKER_SLOW_8S) {
					sei();
				} else { 
					state=S_SLEEP; 	//turning on interrupts to wake up again is taken care of in S_SLEEP
				}
				
				
				break;
			case S_PROGRAMMING:
				
				success = script_run();
				
				if(success) {
					state=S_PROGRAMMING_SUCCESS;
				} else {
					state=S_PROGRAMMING_FAILED;
				}
				break;
			case S_PROGRAMMING_SUCCESS:
				
				buzzer=10;
				
				state=S_IDLE;
				
				break;
			case S_PROGRAMMING_FAILED:
				
				buzzer=50;
				
				state=S_IDLE;
				
				break;
			case S_NO_MORE:
			case S_NO_PROGRAM:
				//nothing to do any more...
				
				break;
				
			case S_SLEEP:
				
				sleep_enable();
				sleep_bod_disable();
				sei();
				sleep_cpu();
				
				//execution is resumed here after processing interrupt
				
				sleep_disable();
				state=S_WAKEUP;
				
				
				break;
		}
		
    }

    return (0);
}


ISR(INT1_vect) {
	//dummy
}

ISR(INT0_vect) {
	//dummy
}