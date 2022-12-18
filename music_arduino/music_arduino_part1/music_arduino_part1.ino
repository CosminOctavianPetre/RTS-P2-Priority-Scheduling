/**********************************************************
 *  INCLUDES
 *********************************************************/
#include "let_it_be_1bit.h"

/**********************************************************
 *  CONSTANTS
 *********************************************************/
// COMMENT THIS LINE TO EXECUTE WITH THE PC
#define TEST_MODE 1

#define SAMPLE_TIME 250 
#define SOUND_PIN  11
#define BUF_SIZE 256
#define PUSH_BUTTON 7
#define LED 13

#define TIME_TASK(TASK) \
    time_exec_begin = micros(); \
    (TASK); \
    time_exec_end = micros(); \
    elapsed = time_exec_end - time_exec_begin;

typedef enum{muted, playing} state_t;
state_t playback_state = playing;
int old_value_button = 0;

/**********************************************************
 *  GLOBALS
 *********************************************************/
unsigned char buffer[BUF_SIZE];
unsigned long timeOrig;

/**********************************************************
 * Function: play_bit
 Worst Compute Time: 28 μs
 *********************************************************/
void play_bit() 
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
          if (Serial.available()>1) {
             data = Serial.read();
          }
       #endif
    }
    switch (playback_state) {
        case playing:
            digitalWrite(SOUND_PIN, (data & bitwise) );
            break;
        case muted:
            digitalWrite(SOUND_PIN, 0);
            break;
    }
}

/**********************************************************
 * Function: read_button_task
 Worst Compute Time: 12 μs
 *********************************************************/
void read_button_task()
{
    int value = digitalRead(PUSH_BUTTON);
    if ( (old_value_button == 0) && (value == 1) ) {
        playback_state = (state_t) !playback_state;
        digitalWrite(LED, !playback_state);      
    }
    old_value_button = value;
}

/**********************************************************
 * Function: setup
 *********************************************************/
void setup ()
{
    // Initialize serial communications
    Serial.begin(115200);

    pinMode(SOUND_PIN, OUTPUT);
    memset (buffer, 0, BUF_SIZE);
    timeOrig = micros();    
}

/**********************************************************
 * Function: loop
 *********************************************************/
void loop ()
{
    unsigned long timeDiff;
    unsigned long time_exec_begin;
    unsigned long time_exec_end, elapsed;

    play_bit();
    TIME_TASK(read_button_task());
    Serial.println(elapsed);
    //timeDiff = SAMPLE_TIME - (micros() - timeOrig);
    //timeOrig = timeOrig + SAMPLE_TIME;
    //delayMicroseconds(timeDiff);
}
