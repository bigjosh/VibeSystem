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

#define LOWBATT_BIT		BIT3		// Low battery signal
#define BUTTON_BIT		BIT6		// HIGH on button press

/*
#define UPDATE_FREQ_HZ		500						// PWM cycle freqency
#define PWM_STEPS			256						// Number of PWM steps per cycle. 0=off, PWM_STEPS-1=on
#define SMCLK_HZ 			1000000					// Device boots to SMCLK=1Mhz

#define UPDATE_COUNT 		(SMCLK_HZ/(UPDATE_FREQ_HZ*PWM_STEPS))	// Value for Timer counter to generate above update freq
*/

volatile unsigned char white_led_PWM;	// 0-255
volatile unsigned char motor_PWM;		// 0-255



// The button_lockout_countdown debounces button presses. If it is anything other than zero, then
// a new button press is ignored, and the countdown is reset to BUTTON_LOCKOUT_TIME.
// As long as the button is released, the countdown will count down towards zero
// ensuring that the button was up for a minimum amount of time before being pressed again.


#define BUTTON_LOCKOUT_TIME	500					// Need button to stay low this long before reset, to debounce

volatile unsigned button_lockout_countdown;		// countdown while button low (pressed)

// The longpress counter waits for the button to be pressed continuously for a minimum
// time period of...

#define LONG_BUTTON_PRESS 20000

volatile unsigned button_longpress_countdown;	// countup while button low (pressed)

volatile unsigned red_led_countdown;			// countdown to turn off red led

static const int speeds[] = { 0 , 16 , 48 + 16  , 204 } ; 	// Fixed speed settings
															// Currently wraps at with 2 bits, so
															// SO must be 4 choices

static unsigned motor_speed_index;			// current speed index


/*
 *  ======== Timer_A2 Interrupt Service Routine ========
 *  Called at UPDATE_FREQ_HZ based on Timer_A2 running in UP mode
 *  Does ghetto PWM
 */
#pragma vector=TIMERA0_VECTOR
__interrupt void TIMERA0_ISR_HOOK(void)
{

	// Note that we check for low batt condition inside the timing loop so we can detect
	// if the battery level drops aschyonously. This will likely happen while the motor is on
	// since that will lower the batt voltage, but could also happen the moment the user turns us
	// on and we are on the first pass through this timing loop, in which case we will turn off
	// without ever turning on the motor


	static unsigned char step;			// Cycle this though to keep track of pwm values

	step++;

	if (!step) {	// Did we overflow?
//		white_led_PWM+=5;
	}

	// I know this is all contorted, but nessisary to minimized generated code size...

	unsigned char port = 0;

	// Simulate PWM by turning on bit when step is less than current PWM setting

	if (step<motor_PWM) {
		port |= MOTOR_BIT;
	}

	if (step<white_led_PWM) {
		port |= WHITE_LED_BIT;
	}

	// Make the LED's show bottom two binary digits of motor state

	DEVICES_OUT = port;

	if (DEVICES_IN & BUTTON_BIT ) {			// button currently pressed?

		if (button_lockout_countdown) {		// Are we currently in a debounce cycle?

			button_lockout_countdown = BUTTON_LOCKOUT_TIME;		// Reset lockout countdown counter; (we need to see it unpress for a min time)

			// Check how long we have been down for

			button_longpress_countdown--;

			// IF we have been down long enough...
			if (!button_longpress_countdown) {

				//we got a long press

				// Turn off motor

				motor_PWM = 0;
				motor_speed_index=0;

				// Which will trigger a power down once the lockot expires.

				button_longpress_countdown = LONG_BUTTON_PRESS;		// Reset the counter so we will loop around and potentially keep turning off, but that is ok.
			}

		}

	} else { // button currently up

		if (button_lockout_countdown) {			// Are we in a debounce?

			button_lockout_countdown--;			// count down...

		}

	}

	// We should only power down if we are not currently debouncing a press otherwise we
	// might immedeately wake up again from bounces

	if (!button_lockout_countdown) {	// Make sure we are not in a lockout and...

		// ...check if the motor got turned off since last

		// Motor could have been turned off by...
		// button cycle to off mode
		// long button press
		// low battery

		if ( !motor_PWM && !white_led_PWM) {		// Motor is off and white LED off (so not charging), so we should goto deep sleep


			// Turn off everything! We can only wake from this mode via a pin change that would signal
			// either a button press or the charger being connected

			//__bis_SR_register_on_exit( LPM4_bits | GIE);

		}
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

	DEVICES_IFG = 0; // Clear all pending flags

	// TODO: Use SVSCTL to detect low battery and eliminate the low battery circuit
/*
	if (DEVICES_IN & LOWBATT_BIT) {		// Low Battery?

		// Blink the red light to indicate low batt (also turns off everything else)

		DEVICES_OUT = RED_LED_BIT;
		unsigned i = 60000;					// SW Delay
		do i--;
		while(i != 0);
		DEVICES_OUT = 0;

		// Then goto deep sleep. Stops timer loop so only a button press can wake us.
		// TODO:__bis_SR_register_on_exit( LPM4_bits | GIE);
		return;
	}
*/

	white_led_PWM += 100;

	if (DEVICES_IN & BUTTON_BIT ) {

		if (button_lockout_countdown==0) {

			motor_speed_index = (motor_speed_index+1) & 0x03;	// Cycle though 4 settings

			motor_PWM = speeds[motor_speed_index];

			button_lockout_countdown = BUTTON_LOCKOUT_TIME;		// Start the countdowntimer for debounce

			button_longpress_countdown = LONG_BUTTON_PRESS;		// Start the counter for a long press

			// If we cycled though to off mode (pwm=0), then we must wait for the lockout ot expire
			// before shutting down or else we might bounce and immedately wake up again.
			// The actuall shutdown code is in the timer routine.

		}
	}

	//__bic_SR_register_on_exit( OSCOFF );		// Clear OSC off bit, which turns OSC on so that timer loop will run

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

	DEVICES_IES = 0;							// Low to high transition interrupt on all pins

	DEVICES_IE = BUTTON_BIT;					// Enable interrupts on the button pin

	//DEVICES_IFG = BUTTON_BIT;							// Software trigger a first pass just to get things going and cover any ints we may have missed

	white_led_PWM=0;

	motor_PWM=0;

	motor_speed_index=0;

	button_lockout_countdown = 0;

	button_longpress_countdown= LONG_BUTTON_PRESS;			// Start waiting to see if this ia longpress

	DEVICES_OUT = 0;		// Start with everything off

	// Sleep and enable the interrupts
	__bis_SR_register( /*LPM4_bits | */ GIE);       // deep sleep w/ interrupt enabled

	while(1);

}
