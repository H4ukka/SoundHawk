/*
    SOUND HAWK rev0 version 0
*/

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>

#include "waves.h"
#include "sound_generator.h"
#include "hardware.h"

// Trigger levels [A] [B]
int trigger_level [2] = { 0 };

// Time variables
unsigned long t_point_a = 0;
unsigned long t_point_b = 0;
unsigned long d_time    = 0;
#define AUTOSET_WAIT      2000

// Channel pins as bytes
#define CHANNEL_A B0001
#define CHANNEL_B B0101

// Trigger access constants and trigger constants
#define CH_A                 0
#define CH_B                 1
#define MAX_TRIGGER_LEVEL 1023

// Serial communication varibales
String serial_input_string = "";
bool serial_string_complete = false;
bool constant_print = false;
bool trig_set_a = false;
bool trig_set_b = false;

// Measurement constants
unsigned short measurement_count = 0;
#define CONST_MEASURMENT_LIMIT 100
#define MEASUREMENT_PAUSE 1000
#define PITCH 24

void setup() {

    /*
        Most of the audio code is based on Jon Thompson's article on makezine.com
        I've merely adopted his methods and molded them to suit my evil plans
        http://makezine.com/projects/make-35/advanced-arduino-sound-synthesis/
    */

    DDRB = DDRB | B11111111;                            // D9 to output mode

    cli();                                              // Disable interrupts

    TCCR1B  = 0;                                        // 
    TCCR1A |= (1 << COM1A1);                            // PWM pin to go low when TCNT1=OCR1A
    TCCR1A |= (1 << WGM10);                             // Put timer into 8-bit fast PWM mode
    TCCR1B |= (1 << WGM12);                             // Set both 10 & 12 for 8 bit mode


    TCCR2A = 0;                                         // We need no options in control register A
    TCCR2B = (1 << CS21);                               // Set prescaller to divide by 8
    TIMSK2 = (1 << OCIE2A);                             // Set timer to call ISR when TCNT2 = OCRA2

    sei();                                              // Enable interrupts

    Serial.begin(9600);

    Serial.print("SOUND HAWK v0 // WELCOME :) \n");     // Print system hello message
    
}

void print_error_msg (String error) {

    Serial.print("----- EXCEPTION DETECTED -----\n");
    Serial.print(error);
}

bool is_triggered (byte channel, short channel_id) {

    if (trigger_level [channel_id] <= adc_convert(channel)) {
        return true;
    }else {
        return false;
    }
}

bool was_triggered_during_wait (byte channel, short channel_id) {
    
    int start_time = millis();
    bool trigd = false;
    
    while (millis() <= (start_time + AUTOSET_WAIT)) {

        if (trigger_level [channel_id] <= adc_convert(channel)) {
            trigd = true;
        }
    
    }
    return trigd;
}

void serial_clear () {
    // Clear serial
    serial_input_string = "";
    serial_string_complete = false;
}

void loop() {

    if (serial_string_complete) {

        // Command detection chain (fugly as heck)
        if (serial_input_string == "Auto Set Triggers\n") {

            // Attempt to auto set triggers
            short return_code = auto_set_trigger_levels ();

            // Process error (if present)
            if (return_code != 0) {
                print_error_msg ("Trigger level passed max value.\n");
            }else{
                Serial.print("Trigger set done \n");
            }

        }else if(serial_input_string == "Print Channel Values\n") {

            // Print trigger levels for both channels
            Serial.print("CHANNEL_A ADC value: ");
            Serial.print(adc_convert(CHANNEL_A));
            Serial.print("\n");

            Serial.print("CHANNEL_B ADC value: ");
            Serial.print(adc_convert(CHANNEL_B));
            Serial.print("\n");

        }else if(serial_input_string == "Print Triggers\n") {

            // Print adc values for both channels
            Serial.print("CHANNEL_A Trigger level: ");
            Serial.print(trigger_level [CH_A]);
            Serial.print("\n");

            Serial.print("CHANNEL_B Trigger level: ");
            Serial.print(trigger_level [CH_B]);
            Serial.print("\n");

        }else if(serial_input_string == "Sound Test\n") {

            // Beep for ACK and test.
            beep ();

        }else if(serial_input_string == "Measure Once\n") {

            OCR2A = PITCH;

            //set_sleep_mode( SLEEP_MODE_ADC );
            //sleep_enable();

            sound_enable ();

            while(trigger_level [CH_A] > adc_convert(CHANNEL_A)) {
                // We do nothing while waiting for channel_a to trigger
                // Add a timeout ?
            }

            //sleep_disable();

            // Channel a triggered! Let's save the number of microseconds counted from program start
            t_point_a = micros();

            //set_sleep_mode( SLEEP_MODE_ADC );
            //sleep_enable();

            while(trigger_level [CH_B] > adc_convert(CHANNEL_B)) {
                // We do nothing while waiting for channel_b to trigger
                // Add a timeout ?
            }

            //sleep_disable();

            // Channel b triggered! Let's save the number of microseconds counted from program start
            t_point_b = micros();

            sound_mute ();

            // Now that both channels have triggered; We can calculate the time difference between them
            d_time = t_point_b - t_point_a;

            // Let's print the result
            Serial.print(millis());
            Serial.print(", ");
            Serial.print(d_time);
            Serial.print(",\n");

            measurement_count++;

        }else if(serial_input_string == "Echo\n") {

            Serial.print(serial_input_string);

        }else if(serial_input_string == "Set Trig Def\n") {

            trigger_level [CH_A] = 50;
            trigger_level [CH_B] = 50;

        }else if(serial_input_string == "Set Trig A\n") {

            Serial.print("Give trigger value for channel A\n");
            trig_set_a = true;

        }else if(serial_input_string == "Set Trig B\n") {

            Serial.print("Give trigger value for channel B\n");
            trig_set_b = true;

        }else if(trig_set_a) {

            trigger_level [CH_A] = serial_input_string.toInt();
            Serial.print("Trigger value for channel A set to: ");
            Serial.print(trigger_level [CH_A]);
            Serial.print("\n");

            trig_set_a = false;

        }else if(trig_set_b) {
            
            trigger_level [CH_B] = serial_input_string.toInt();
            Serial.print("Trigger value for channel B set to: ");
            Serial.print(trigger_level [CH_B]);
            Serial.print("\n");

            trig_set_b = false;

        }else if(serial_input_string == "ON\n") {

            sound_enable();
            OCR2A = PITCH;

        }else if(serial_input_string == "OFF\n") {

            sound_mute ();

        }else if(serial_input_string == "Show Const\n") {

            constant_print = true;

        }else if(serial_input_string == "Dis Const\n") {

            constant_print = false;

        }else if(serial_input_string == "Measure Full\n") {

            measurement_count = 0;

            while (measurement_count < CONST_MEASURMENT_LIMIT) {

                OCR2A = PITCH;

                //set_sleep_mode( SLEEP_MODE_ADC );
                //sleep_enable();

                sound_enable ();

                while(trigger_level [CH_A] > adc_convert(CHANNEL_A)) {
                    // We do nothing while waiting for channel_a to trigger
                    // Add a timeout ?
                }

                // Channel a triggered! Let's save the number of microseconds counted from program start
                t_point_a = micros();

                while(trigger_level [CH_B] > adc_convert(CHANNEL_B)) {
                    // We do nothing while waiting for channel_b to trigger
                    // Add a timeout ?
                }

                //sleep_disable();

                // Channel b triggered! Let's save the number of microseconds counted from program start
                t_point_b = micros();

                delay (20);

                sound_mute ();

                // Now that both channels have triggered; We can calculate the time difference between them
                d_time = t_point_b - t_point_a;

                // Let's print the result
                Serial.print(millis());
                Serial.print(", ");
                Serial.print(d_time);
                Serial.print(",\n");

                measurement_count++;

                delay (MEASUREMENT_PAUSE);
            }
        }else {

            // Unknown command
            Serial.print("command " + serial_input_string + "was not recognized.\n");
        }
        
        serial_clear ();

    }else if (constant_print) {

        Serial.print(adc_convert(CHANNEL_A));
        Serial.print(" / ");
        Serial.print(adc_convert(CHANNEL_B));
        Serial.print("\n");

        delay (200);
    }
}

// Read input from the serial line as whole strings

void serialEvent() {

    while(Serial.available()) {

        char char_in = (char) Serial.read();
        serial_input_string += char_in;

        if (char_in == '\n') {
            serial_string_complete = true;
        }
    }
}

short auto_set_trigger_levels () {

    // Reset before start
    trigger_level [CH_A] = 0;
    trigger_level [CH_B] = 0;

    // While the ADC readout is greater than the trigger; increase trigger
    // If trigger passes 1023; Something went worng and we abort
    // Repeat until channel does not trigger during the wait time
    do {
        while (is_triggered(CHANNEL_A, CH_A)) {

            trigger_level [CH_A]++;

            if (trigger_level [CH_A] > MAX_TRIGGER_LEVEL) {
                return 2;
            }
        }
    }while(was_triggered_during_wait(CHANNEL_A, CH_A));

    // Exactly the same as above except for channel b
    do {
        while (is_triggered(CHANNEL_B, CH_B)) {

            trigger_level [CH_B]++;

            if (trigger_level [CH_B] > MAX_TRIGGER_LEVEL) {
                // This number could be anything except 0
                return 2;
            }
        }
    }while(was_triggered_during_wait(CHANNEL_B, CH_B));

    // 0 is the default value for "success"
    return 0;
}

/*
    This Interrupt Service Routine is used to load the next sample from the currently used wavetable
    
    TCNT2 has to be adjusted at the end; otherwise we're going to sound terrible
    '6' seems to be the right magical number for compensating the time we spent in the ISR call

    The index variable stores the index value of the current sample in the wavetable
    During each ISR call we increment it by 1 to laod the next sample
    When the index value reaches 255 it will overflow back to 0 since it's defined as a byte
*/

ISR(TIMER2_COMPA_vect) {

    static byte sample = 0;

    OCR1AL = pgm_read_byte(&WAVES[SQUARE_DIGITAL][sample++]);
    TCNT2 = 6;
}