/*
 * EmbeddedProject.c
 *
 * Created: 10-3-2016 12:32:59
 * Author : Jasper
 */ 

#include "rp6aansluitingen.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define FWD 0
#define BWD 1

/*				*
****functies*****
*				*/
void rijVooruit();
void rijAchteruit();
void naarLinks();
void naarRechts();
void testCycle();
void wait(uint8_t seconden);
void stopDriving();
void incrementSpeed();
void decrementSpeed();

//setters
void setMotorPower(uint8_t right, uint8_t left);
void setMotorPowerDynamic(uint8_t right_des, uint8_t left_des);
void setMotorSpeed(float speedRight, float speedLeft);
void setMotorDirection(uint8_t left, uint8_t right);

//getters
float getDistanceByInterrupts(uint8_t interrupts);
float getTotalDistance();

//initialisering
void init();
void dynamicUpdate();

/*				 *
****variabelen****
*				 */

//power on the motor
uint8_t curPower_left = 0;
uint8_t curPower_right = 0;
uint8_t snelheid = 0;

//current motor speed in mm/s
float curSpeed_left = 0;
float curSpeed_right = 0;
float desiredSpeed_left = 0;
float desiredSpeed_right = 0;

//timers
#define SPEED_TIMER_TRIGGER 200 //5x/sec
uint16_t speed_timer = 0;
uint16_t update_timer = 0;
uint8_t ms_timer = 0;

//motor direction
uint8_t curDirection_left = 0;
uint8_t curDirection_right = 0;

//distance the device has traveled in # of interrupts
uint8_t motorDistance_left = 0;
uint8_t motorDistance_right = 0;
uint8_t motorDistanceLast_left = 0;
uint8_t motorDistanceLast_right = 0;
uint8_t motorDistanceTotal_left = 0;
uint8_t motorDistanceTotal_right = 0;

int main(void)
{	
	init();
    while (1) 
    {
		testCycle();
    }
	return(0);
}

void testCycle(){
	wait(2);
	rijVooruit();
	
	wait(2);
	naarLinks();
	
	wait(2);
	rijAchteruit();	
	
	wait(2);
	naarRechts();
	
	if(snelheid > 210)
		snelheid = 0;
	else
		snelheid += 25;
}

void wait(uint8_t seconden){
	for(int i = 0; i < 3; i++)
		_delay_ms(250);
}

void init(){
	cli();
	
	//set pins I/O
	//DDRA = 0x00; //00000000
	//DDRB =  //11011011
	DDRC= DIR_R | DIR_L; //111111xx
	DDRD = MOTOR_R | MOTOR_L; //01110010
	
	//initialize PWM (timer1)
	TCCR1A = (0 << WGM10) | (1 << WGM11) | (1 << COM1A1) | (1 << COM1B1);
	TCCR1B =  (1 << WGM13) | (0 << WGM12) | (1 << CS10);
	ICR1 = 210; 
	OCR1AL = 0;
	OCR1BL = 0;
	setMotorDirection(FWD, FWD);
	
	//initialize interrupts: int0 links, int1 rechts
	MCUCR = (0 << ISC11) | (1 << ISC10) | (0 << ISC01) | (1 << ISC00);
	GICR = (1 << INT2) | (1 << INT1);

	//initialiseer Timer 0: 100�s cycle
	TCCR0 =   (0 << WGM00) | (1 << WGM01)				//Counter mode:CTC Top:OCR0 Update:Immediate TOV0flag set on:MAX
			| (0 << COM00) | (0 << COM01)				//normal port OC0 disconnected
			| (0 << CS02)  | (1 << CS01) | (0 << CS00); //8bit prescaler
	OCR0  = 99;											//output compare register
		
	sei();
}

//always call this to maintain speed control OBSOLETE
void dynamicUpdate(){
	//amend motor speeds
	if(curSpeed_right < desiredSpeed_right) curPower_right++;
	if(curSpeed_left < desiredSpeed_left) curPower_left++;
	if(curSpeed_right > desiredSpeed_right) curPower_right--;
	if(curSpeed_left > desiredSpeed_left) curPower_left--;
	setMotorPower(curPower_right, curPower_left);
	
	if(curPower_left || curPower_right)
	TCCR1A = (1 << WGM11) | (1 << COM1A1) | (1 << COM1B1);
	else
	TCCR1A = 0;
}

void setMotorPower(uint8_t right, uint8_t left){
	if(right > 210) right = 210;
	if(left > 210) left = 210;
	OCR1AL = right;
	OCR1BL = left;
}

//sets the desired speed of the left and right motor which is adapted dynamically through the dynamicUpdate method
void setMotorSpeed(float speedRight, float speedLeft){// obsolete
	if(speedRight < 50)
	desiredSpeed_right = 0;
	if(speedRight > 1200)
	desiredSpeed_right = 1200;
	else
	desiredSpeed_right = speedRight;

	if(speedLeft < 50)
	desiredSpeed_left = 0;
	if(speedLeft > 1200)
	desiredSpeed_left = 1200;
	else
	desiredSpeed_left = speedLeft;
}

void setMotorPowerDynamic(uint8_t right_des, uint8_t left_des){
	while(right_des != curPower_right && left_des != curPower_left){
		if(right_des < curPower_right) curPower_right--;
		if(right_des > curPower_right) curPower_right++;
		if(left_des < curPower_left) curPower_left--;
		if(left_des > curPower_left) curPower_left++;
		setMotorPower(curPower_right, curPower_left);
		_delay_ms(10);
	}
}

void incrementSpeed(){
	snelheid ++;
}

void decrementSpeed(){
	snelheid --;
}

//sets the direction of the left and right motor respectively, only call when speed = 0
void setMotorDirection(uint8_t left, uint8_t right){
	if(left)
		PORTC |= DIR_L;
	else
		PORTC &= ~DIR_L;
		
	if(right)
		PORTC |= DIR_R;
	else
		PORTC &= ~DIR_R;
		
	curDirection_right = right;
	curDirection_left = left;
}

void rijVooruit(){
	if(!(curDirection_left == FWD && curDirection_right == FWD)){//zet snelheid naar 0 en verander de richting als dat nodig is
		setMotorPowerDynamic(0, 0);
		setMotorDirection(FWD,FWD);		
	}
	setMotorPowerDynamic(snelheid, snelheid);
}

void rijAchteruit(){
	if(!(curDirection_left == BWD && curDirection_right == BWD)){//zet snelheid naar 0 en verander de richting als dat nodig is
		setMotorPowerDynamic(0, 0);
		setMotorDirection(BWD,BWD);		
	}
	setMotorPowerDynamic(snelheid, snelheid);
}

void naarLinks(){
	if(!(curDirection_left == BWD && curDirection_right == FWD)){//zet snelheid naar 0 en verander de richting als dat nodig is
		setMotorPowerDynamic(0, 0);
		setMotorDirection(BWD,FWD);
	}
	setMotorPowerDynamic(snelheid, snelheid);
	//TODO gebruik kompas
}

void naarRechts(){
	if(!(curDirection_left == FWD && curDirection_right == BWD)){//zet snelheid naar 0 en verander de richting als dat nodig is
		setMotorPowerDynamic(0, 0);
		setMotorDirection(FWD,BWD);
	}
	setMotorPowerDynamic(snelheid, snelheid);
	//TODO gebruik kompas
}

void stopDriving(){
	setMotorPowerDynamic(0,0);
}

//each interrupt = .25 mm, therefore this returns #interrupts *.25
float getDistanceByInterrupts(uint8_t interrupts){
	return interrupts * 0.25; //return distance in mm
}

//returns the total driven distance
float getTotalDistance(){
	return getDistanceByInterrupts((motorDistanceTotal_right + motorDistanceTotal_left)/2);
}

//external interrupt int0 left motor sensor
ISR (INT0_vect){
	motorDistance_left++;	//increment the amount of interrupts on the left side

	if(curDirection_right == curDirection_left)//if the car is not rotating increment the total distance
		motorDistanceTotal_left++;
}

//external interrupt int1 right motor sensor
ISR (INT1_vect){
	motorDistance_right++;	//increment the amount of interrupts on the right side

	if(curDirection_right == curDirection_left)//if the car is not rotating increment the total distance
		motorDistanceTotal_right++;
}

//timer interrupt for calc purposes
ISR (TIMER0_COMP_vect){
	
	if(ms_timer++ >= 9) {//1 interrupt per ms
		
		//calculate speed of both sides TODO maybe extra
		/*
		if(speed_timer++ > SPEED_TIMER_TRIGGER){
			curSpeed_right = motorDistance_right; //getDistanceByInterrupts(motorDistance_right) * 5; //in mm/s
			curSpeed_left = motorDistance_left; //getDistanceByInterrupts(motorDistance_left) * 5;	 //in mm/s
		
			//set the last distances to the current distances so the delta can be used next cycle
			motorDistanceLast_left = motorDistance_left;
			motorDistanceLast_right = motorDistance_right;
			motorDistance_right = 0;
			motorDistance_left = 0;
			speed_timer = 0;	//reset the timer so it triggers again	
		}
	
		if(update_timer++ > 2){
			dynamicUpdate();
			update_timer = 0;
		}*/
		
		ms_timer = 0;
	}
}