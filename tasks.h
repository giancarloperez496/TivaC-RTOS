// Tasks
// J Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef TASKS_H_
#define TASKS_H_

#define BLUE_LED PORTF,2
#define GREEN_LED PORTE,0
#define YELLOW_LED PORTA,4
#define ORANGE_LED PORTA,3
#define RED_LED PORTA,2
#define PB0 PORTC,4
#define PB1 PORTC,5
#define PB2 PORTC,6
#define PB3 PORTC,7
#define PB4 PORTD,6
#define PB5 PORTD,7

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void initHw(void);

void idle(void);
void flash4Hz(void);
void oneshot(void);
void partOfLengthyFn(void);
void lengthyFn(void);
void readKeys(void);
void debounce(void);
void uncooperative(void);
void errant(void);
void important(void);

#endif
