/**********************************************************
 *  INCLUDES
 *********************************************************/
#include "let_it_be_1bit.h"


/**********************************************************
 *  CONSTANTS
 *********************************************************/
// COMMENT THIS LINE TO EXECUTE WITH THE PC
// #define TEST_MODE 1

#define SAMPLE_FREQ     3999
#define BUF_SIZE        256

// ------------------------------------
// Digital PIN Numbers
// ------------------------------------
#define PUSH_BUTTON     7
#define SOUND           11
#define LED             13


/**********************************************************
 *  TYPEDEFS
 *********************************************************/
typedef enum{muted, playing} state_t;


/**********************************************************
 *  GLOBALS
 *********************************************************/
unsigned char buffer[BUF_SIZE];
state_t playback_state = playing;
int old_value_button = 0;


// ------------------------------------
// Function Prototypes
// ------------------------------------

/************** Tasks ****************/

// ------------------------------------
// Task: Playback
// Worst Compute Time: 28 μs
// ------------------------------------
void task_playback();
// ------------------------------------
// Tasks: Read Button and Display LED
// Worst Compute Time: 24 μs
// ------------------------------------
void task_button_led();


/************** Tasks ****************/
void task_playback() 
{
    static int bitwise = 1;
    static unsigned char data = 0;
    static int music_count = 0;

    bitwise = (bitwise * 2);
    if (bitwise > 128) {
        bitwise = 1;
#ifdef TEST_MODE 
    data = pgm_read_byte_near(music + music_count);
    music_count = (music_count + 1) % MUSIC_LEN;
#else 
    if (Serial.available() > 1) {
        data = Serial.read();
    }
#endif
    }
    switch (playback_state) {
        case playing:
            digitalWrite(SOUND, (data & bitwise) );
            break;
        case muted:
            digitalWrite(SOUND, 0);
            break;
    }
}


void task_button_led()
{
    int value = digitalRead(PUSH_BUTTON);
    if ( (old_value_button == 0) && (value == 1) ) {
        noInterrupts();
        playback_state = (state_t) !playback_state;
        digitalWrite(LED, !playback_state);
        interrupts();
    }
    old_value_button = value;
}


void setup()
{
    // Initialize serial communications
    Serial.begin(115200);
    memset(buffer, 0, BUF_SIZE);

    // set pins
    pinMode(SOUND, OUTPUT);
    pinMode(LED, OUTPUT);
    pinMode(PUSH_BUTTON, INPUT);

    // set up timer 1 for interrupts
    TCCR1A = _BV(COM1A0);
    TCCR1B = _BV(WGM12) | _BV(CS10);
    TIMSK1 = _BV(OCIE1A);

    OCR1A = SAMPLE_FREQ;
    OCR1B = 0;
}


// Interrupt handler
ISR(TIMER1_COMPA_vect)
{
    task_playback();
}


void loop()
{
    task_button_led();
}
