#include <msp430.h>


// All devices on the same port for bit efficiency...
#define DEVICES_DIR	P1DIR
#define DEVICES_OUT	P1OUT
#define DEVICES_IN	P1IN
#define DEVICES_IFG	P1IFG			// Interrupt flag reg
#define DEVICES_IES	P1IES			// Interrupt edge select, 0=interrupt on low to high
#define DEVICES_IE	P1IE			// INterrupt enable on pin

// Outputs
#define RED_LED_BIT 	BIT4
#define WHITE_LED_BIT 	BIT5
#define MOTOR_BIT 		BIT1

//Inputs
#define CHARGE_BIT 		BIT0		// LOW indicates charge in progress
#define PG_BIT			BIT2		// LOW indicates Power Good (charer is seeing power)
#define EOG_BIT			BIT7		// End of charge (battery full)

#define LOWBATT_BIT		BIT3		// Low battery signal, goes to 0 when batt volatge drops below 3*1.2 = 3.6 volts
#define BUTTON_BIT		BIT6		// HIGH on button press

/*
#define UPDATE_FREQ_HZ		500						// PWM cycle freqency
#define PWM_STEPS			256						// Number of PWM steps per cycle. 0=off, PWM_STEPS-1=on
#define SMCLK_HZ 			1000000					// Device boots to SMCLK=1Mhz

#define UPDATE_COUNT 		(SMCLK_HZ/(UPDATE_FREQ_HZ*PWM_STEPS))	// Value for Timer counter to generate above update freq
*/


// The button_lockout_countdown debounces button presses. If it is anything other than zero, then
// a new button press is ignored, and the countdown is reset to BUTTON_LOCKOUT_TIME.
// While the button is released, the countdown will count down towards zero
// ensuring that the button was up for a minimum amount of time before being pressed again.

#define BUTTON_LOCKOUT_TIME	500					// Need button to stay low this long before reset, to debounce

volatile unsigned button_lockout_countdown;		// countdown while button low (pressed)

// The longpress counter waits for the button to be pressed continuously for a minimum
// time period of...

#define LONG_BUTTON_PRESS 20000

volatile unsigned button_longpress_countdown;	// count down while button low (pressed)

//volatile unsigned red_led_countdown;			// countdown to turn off red led

#define MOTOR_SPEED_COUNT 4
static const int motor_speeds[MOTOR_SPEED_COUNT] = { 0 , 16 , 48 + 16  , 204 } ; 	// Fixed speed settings


volatile unsigned motor_speed_index;			// current speed index

volatile unsigned powerup_flag;				// Are we just waking from a powerup?

volatile unsigned low_batt_detected_flag;	// If we saw low voltage for a moment, power down even if it comes back up and require a button press to Wake again


/*
 *  ======== Timer_A2 Interrupt Service Routine ========
 *  Called at UPDATE_FREQ_HZ based on Timer_A2 running in UP mode
 *  Does ghetto PWM
 */
#pragma vector=TIMERA0_VECTOR
__interrupt void TIMERA0_ISR_HOOK(void)
{

	static unsigned char pulse;		// Slowly pulses

	DEVICES_OUT = 0;			// Turn everything off for a moment
								// DO this first to let the battery voltage rise a bit without load before we check it.

	static unsigned char step;			// Cycle this though to keep track of pwm values

	step++;

	if (!step) {	// Did we overflow?

		// Generate the variable that we use elsewhere for pulsing...

		static int pulse_dir;

		if (pulse<=5) {
			pulse_dir = 1;
		} else if (pulse>=70) {
			pulse_dir = -1;
		}

		pulse += pulse_dir;

	}

	unsigned powerdown_flag=0;			// shoud we turn off?

	// Compute the PWM channels based on current state
	// All channels start at zero default then we change them as nessisary

	unsigned char white_led_PWM=0;	// 0-255

	unsigned char motor_PWM=0;		// 0-255

	unsigned char red_led_PWM=0;	// 0-255


	if ( !(DEVICES_IN & PG_BIT) ) {					// Charger connected

		motor_speed_index = 0;						// Turn off motor when charger connected  TODO: is this correct UI?

		if ( !(DEVICES_IN & EOG_BIT) ) {			// Charger battery full?

			white_led_PWM = 255;

		} else if ( !( DEVICES_IN & CHARGE_BIT)) {	// Charge in progress

			white_led_PWM = pulse;

		} else {

			white_led_PWM = 100;
			red_led_PWM = pulse;
		}

		// Note that we will never power down if charger connected, so LEDs can show charging status

	} else {										// No charger connected

		// We check for low batt condition inside the timing loop so we can detect
		// if the battery level drops aschyonously. This will likely happen while the motor is on
		// since that will lower the batt voltage, but could also happen the moment the user turns us
		// on and we are on the first pass through this timing loop, in which case we will turn off
		// without ever turning on the motor

		// TODO: Use SVSCTL to detect low battery and eliminate the low battery circuit and get software control over setpoint

		if ( !(DEVICES_IN & LOWBATT_BIT)) {			// Low Battery detected?

			low_batt_detected_flag = 1;

		}

		if (low_batt_detected_flag) {

			red_led_PWM=200;						// Show red LED to user (they will only see if button pressed becuase otherwsie we will power down immedeately)

			motor_speed_index = 0;					// Turn off motor (which will lead to powerdown)

		}

		if (motor_speed_index==0) {					// Motor currently off?

			powerdown_flag = 1;						// Powerdown when button is released

		}

		motor_PWM = motor_speeds[ motor_speed_index ];

	}


	if (button_lockout_countdown) {		// Are we currently in a debounce cycle?

		if (DEVICES_IN & BUTTON_BIT) {			// button currently pressed?

			button_lockout_countdown = BUTTON_LOCKOUT_TIME;		// Reset lockout countdown counter since button is pressed right now; (we need to see it unpress for a min time)

			if (button_longpress_countdown) {					// Currently in a longpress countdown?

				// Check how long we have been down for

				button_longpress_countdown--;

				// If we have been down long enough...

				if (!button_longpress_countdown) {

					// Turn off motor immedeately for user feedback

					motor_speed_index = 0;					// Goto off mode just in case battery level rises again while button down, we will need to button cycle to turn on again

					// then power down when button released

					powerdown_flag = 1;
				}
			}

		} else { // button currently up

			// We know button_lockout_countdown>0 if we get here from enclosing if

			button_lockout_countdown--;			// count down...

			button_longpress_countdown = LONG_BUTTON_PRESS;		// Start waiting for long press from scatch (we need to see continuously press)

		}

	}


	// We should only power down if we are not currently debouncing a press otherwise we
	// might immedeately wake up again from bounces

	if (!button_lockout_countdown && powerdown_flag) {	// Make sure we are not in a debound lockout - button has been safely released

		// Remeber that all outputs are low right now because we turned them off at the
		// top of the routine.

		// We can only wake from this mode via a pin change that would signal
		// either a button press or the charger being connected

		powerup_flag   = 1;			// A global to tell the int route to init variables on powerup.

		__bis_SR_register_on_exit( LPM4_bits | GIE);

		// Good night!

		return;

	}

	// Remeber that when we get here, all devices are still off becuase we turnted them off at the
	// top of this routine

	// Simulate PWM by turning on bit when step is less than current PWM setting

	if (step<motor_PWM) {
		DEVICES_OUT|= MOTOR_BIT;
	}

	if (step<white_led_PWM) {
		DEVICES_OUT|= WHITE_LED_BIT;
	}

	if (step<red_led_PWM) {

		DEVICES_OUT|= RED_LED_BIT;

	}


}

/*
 * ===== Port 1 interrupt service routine====
 *
 * Handles:
 * 	button press
 *
 */



// This vector is called when...
//
// The button is pressed

#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void)
{

	// Docs are unclear on how to atomically read and reset flags.
	// I think this is the safest possible way.

	DEVICES_IFG = 0; // Clear all pending flags

	if (powerup_flag) {		// Are we waking from a powerdown?

		motor_speed_index=0;	// Start mode cycle over

		// TODO: Need next two lines? Or does it happen naturally?

		button_lockout_countdown = 0;

		low_batt_detected_flag = 0;

		powerup_flag = 0;		// Clear powerdown condition until next time

	}

	if ( DEVICES_IN & BUTTON_BIT ) {

		if (button_lockout_countdown==0) {

			motor_speed_index++;

			if (motor_speed_index == MOTOR_SPEED_COUNT) {	// Did we cycle to the end?

				motor_speed_index=0;		// Switch to off mode. This will also power us down in the timer routine when button lockout completes

			}

			button_lockout_countdown = BUTTON_LOCKOUT_TIME;		// Start the countdowntimer for debounce

			button_longpress_countdown = LONG_BUTTON_PRESS;		// Start the counter for a long press


		}
	}

	__bic_SR_register_on_exit( LPM4_bits );		// Clear LMP4 bits so we are ruuning normal mode, which turns OSC on so that timer ISR will run
												// TODO: Find lowest power mode that lets timer run, although tis doesn't matter much since
												//       it only effect power usage while we are on and inbetween timer calls

}


/*
 *
 * This is the entry point from the ASM start up. It is *not* like main - we do not have any
 * assumptions when we get here. No variables are in a known state and nothting is set up
 * except for the start pointer. Do not Return from this procedure becuase there is no
 * place to return from.
 *
 */

void cstart(void)
{
	WDTCTL = WDTPW + WDTHOLD;                 // Stop watchdog timer

	// Speed up the clock...

	BCSCTL1 = XT2OFF | RSEL3 | RSEL2 | RSEL1 ; 		// XT2OFF, rsel = 14;
	DCOCTL = DCO2 |DCO1 | DCO0;						// DCO = 7;

	//Now running at 15.197mhz - fast enough for manual PWM bit banging

	// Setup Pins

	// Set all device pins to output mode

//	DEVICES_OUT = 0;
	DEVICES_DIR = WHITE_LED_BIT | RED_LED_BIT | MOTOR_BIT;


	P2OUT = 0x00;		// Port 2 pins unused, so so set to output low to save power.
	P2DIR = 0xFF;

	// Set up Timer

	// The Timer on this part is very limiting so we need to be creative.
	// We will set the timer0 to generate an interrupt at 5Khz and we will use that to do manual PWM in the ISR

	TACTL = TASSEL_2 | MC_1;           // Timer_A Control Register

									  // TASSEL       = 10   Timer_A Source = SMCLK
									  // ID           = 00   Input divider /1
									  // MC           = 01   Up mode, count up to TACCR0 and roll over

	TACCR0 = 500;      					// Timer_A Compare Register 1
										// We will generate an interrupt each time we get up to this value

	TACCTL0 = CCIE;						// Enable interrupt on reaching TACCR0

	// Set up pin interrupts

	DEVICES_IFG = 0;							// Clear any pending flags

	DEVICES_IES = PG_BIT;						// Low to high transition interrupt on button, high to low on charger connection (it is active low)

	DEVICES_IE = BUTTON_BIT | PG_BIT;			// Enable interrupts on the button pin or charger connection


	DEVICES_IFG = BUTTON_BIT;					// Software trigger a first pass just to get things going and cover any ints we may have missed
												// For example - we just powered up becuase charger was attached

	DEVICES_OUT = 0;		// Start with everything off

	powerup_flag = 1; 	// Start up fresh on next interrupt

	// Sleep and enable the interrupts
	__bis_SR_register( LPM4_bits | GIE);       // deep sleep w/ interrupt enabled

	while(1);								   // We should never get here....

}
