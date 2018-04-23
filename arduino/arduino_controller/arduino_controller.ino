#include "CmdMessenger.h"
#include "Servo.h"


/*
 * Define which pins will be input / output depending on what board is used. 
 * Mostly needed right now to programand test on a Nano and install on the Leonardo
 *
 * The array of pins should be static, these determine the physical hookups.
 * The input and output enumbs, defined below, will always pull from the same array
 * position.  This is why the array values must stay the same, they just pass through the 
 * connection to the physical board.
 */

#define NUM_INPUT 3
#define NUM_OUTPUT 3
#define NUM_ANALOG 3

#define SERVO_MAX_ANGLE 75
#define SERVO_MIN_ANGLE 30

#if defined(__AVR_ATmega328P__)
const int input_pins[NUM_INPUT] = {5, 6, 7};
const int output_pins[NUM_OUTPUT] = {2, 3, 4};
const int analog_pins[NUM_ANALOG] = {A0, A1, A2};
#endif

#if defined(__AVR_ATmega32U4__)
const int input_pins[NUM_INPUT] = {9, 10, 11};
const int output_pins[NUM_OUTPUT] = {5, 6, 7};
const int analog_pins[NUM_ANALOG] = {ADC0, ADC1, ADC2};
#endif

/* firmware version */
const static int firmware_version[3] = {0, 1, 0};

/* Define available CmdMessenger commands */
enum {
    ping,
    pong,
    req_firmware,
    send_firmware,
    player,
    lights,
    get_state,
    ret_state,
    release_latches,
    error
};

/*
 * The inputs and outputs enumos define which buttons are attached to which physical pins as defined
 * int the input_pins and output_pins arrays above.
 */

enum inputs {
    pir,              // Infrared sensor
    blank,
    stream_button     // big green stream button
};

enum ouptuts {
    stream_button_light,  // output to control the stream button light
    set_webcam_angle
};

enum analogs {
    read_webcam_angle
};

Servo webcam_angle;       // Servo to adjust webcam angle

/* 
 * Button handling 
 * The buttons need debouncing, and values ma or may not need latchhing.  
 * FIXME: need to add a "NUM_INPUT_PIN" variable at some point., but need to do all that
 * when i get some further specs, so will wait.
 */
uint8_t pin_latch_value[NUM_INPUT];
uint8_t pin_latched[NUM_INPUT];
uint8_t pin_active[NUM_INPUT];
uint8_t pin_state[NUM_INPUT];

int light_state = LOW;
int pin = 0;            // pin interation


/* Initialize CmdMessenger -- this should match PyCmdMessenger instance */
const int BAUD_RATE = 9600;
CmdMessenger c = CmdMessenger(Serial,',',';','/');

/* Create callback functions to deal with incoming messages */

void do_pong(void){
    c.sendCmd(pong, "pong");
}

void do_send_firmware(void) {
    c.sendCmdStart(send_firmware);
    c.sendCmdBinArg(firmware_version[0]);  // major number
    c.sendCmdBinArg(firmware_version[1]);
    c.sendCmdBinArg(firmware_version[2]);
    c.sendCmdEnd();
}

void send_state(void){
    c.sendCmdStart(ret_state);
    for(pin=0; pin<NUM_INPUT; pin++) {
        c.sendCmdBinArg((int)pin_state[pin]);
    }
    for(pin=0; pin<NUM_INPUT; pin++) {
        c.sendCmdBinArg((int)digitalRead(output_pins[pin]));
    }
    for(pin=0; pin<NUM_INPUT; pin++) {
        c.sendCmdBinArg((int)pin_latched[pin]);
    }
    for(pin=0; pin<NUM_INPUT; pin++) {
        c.sendCmdBinArg((int)pin_latch_value[pin]);
    }
    c.sendCmdEnd();
}

void is_player(void){
    c.sendBinCmd(player, (int)pin_state[pir]);
}

void lights_handler(void){
    int param1 = c.readBinArg<bool>();
    // int param2 = c.readBinArg<int>();

    if(param1 == 0) {
        light_state = LOW;
    } else {
        light_state = HIGH;
    }

    for(pin=0; pin<NUM_INPUT; pin++) {
        digitalWrite(output_pins[pin], light_state);
    }
    send_state();
}

void on_unknown_command(void){
    c.sendCmd(error,"Command without callback.");
}

void unlatch_pins(){
    for(pin=0;pin<NUM_INPUT;pin++){
        pin_latched[pin]=0;
        pin_latch_value[pin]=0;
    }
    send_state();
}

/* Attach callbacks for CmdMessenger commands */
void attach_callbacks(void) {
    c.attach(ping, do_pong);
    c.attach(req_firmware, do_send_firmware);
    c.attach(player, is_player);
    c.attach(lights, lights_handler);
    c.attach(get_state, send_state);
    c.attach(on_unknown_command);
    c.attach(release_latches, unlatch_pins);
}

/*
 * Button management.  All the reading, debouncings, latchings happens here.
 * the debounce is handled in two stages, a simulated RC filter and a schmitt trigger.
 */
void read_btns(void) {
    static uint8_t y_old[NUM_INPUT]={0,0,0};
    int state_change = 0;

    for(pin=0; pin<NUM_INPUT; pin++) {
        y_old[pin]=0;
    }

    for(pin=0;pin<NUM_INPUT;pin++){
        y_old[pin] = y_old[pin] - (y_old[pin] >> 2);

        if(digitalRead(input_pins[pin])){y_old[pin] = y_old[pin] + 0x3F;}

        if((y_old[pin] > 0xF0)&&(pin_state[pin]==0)) {
            pin_state[pin]=1;
            state_change=1; 
        }
        if((y_old[pin] < 0x0F)&&(pin_state[pin]==1)) {
            pin_state[pin]=0;
            state_change=1;
        }
        // handle this better for active high/low switches.
        if((state_change==1)&&(pin_latched[pin]==0)&&pin_state[pin]==pin_active[pin]) {
            pin_latch_value[pin]=pin_active[pin];
            pin_latched[pin]=1;
        }
        state_change=0;
    }
}

/* 
 * Adjust servo height.  Input us an analog signal between ground and VCC+, constrained
 * by the values which make the camera visible.  If I get bored, I'll write a calibration routine.
 * easiest adjustor is a slider/knob/rocker potentiometer.
 */
void adjust_webcam_angle() {
    int val;
    val = analogRead(analog_pins[read_webcam_angle]);
    val = map(val, 0, 1023, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
    webcam_angle.write(val);
}

void setup() {
    Serial.begin(BAUD_RATE);

    attach_callbacks();

    for(pin=0; pin<NUM_INPUT; pin++) {
        pin_latch_value[pin] = 0;
        pin_latched[pin] = 0;
        pin_active[pin] = 1;
        pin_state[pin] = 1;
    }

    for(pin=0; pin<NUM_INPUT; pin++) {
        pinMode(input_pins[pin], INPUT_PULLUP);
    }

    for(pin=0; pin<NUM_ANALOG; pin++) {
        pinMode(analog_pins[pin], INPUT);
    }

    for(pin=0; pin<NUM_OUTPUT; pin++) {
        pinMode(output_pins[pin], OUTPUT);
        digitalWrite(output_pins[pin], LOW);
    }
}

void loop() {
    c.feedinSerialData();
    read_btns();
}

