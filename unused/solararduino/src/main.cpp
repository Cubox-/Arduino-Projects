#include <Arduino.h>

#include <LowPower.h>
#define EI_NOTEXTERNAL
#include <EnableInterrupt.h>
#define ENABLE_LOOP_ITERATION
#include <SoftTimer.h>
#include <DelayRun.h>
#define NUM_SAMPLES 200

#define TRIGGER_VOLTAGE 21
#define TRIGGER_DELAY 600000

unsigned long lastTriggerSwitch = 0;

enum overrideStates {
    NORMAL,
    PENDING,
    FORCEDON,
    FORCEDOFF
};

enum overrideStates overrideState = NORMAL;

void checkVoltageAndAct(Task*);

void overridePressed() {
    if (overrideState == NORMAL) {
        overrideState = PENDING;
    } else if (overrideState == FORCEDON) {
        overrideState = FORCEDOFF;
    } else if (overrideState == FORCEDOFF) {
        overrideState = FORCEDON;
    }

    disableInterrupt(12);
    LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_ON);
    enableInterrupt(12, overridePressed, FALLING);
    checkVoltageAndAct(NULL);
}

void setState(bool state) {
    digitalWrite(3, state);
    digitalWrite(LED_BUILTIN, state);
}

double measureVoltage() {
    unsigned long sum = 0; // sum of samples taken
    unsigned char sample_count = 0; // current sample number
    double voltage = 0.0; // calculated voltage
    while (sample_count < NUM_SAMPLES) {
        sum += analogRead(A1);
        sample_count++;
    }
    // Here the 5 is calibrated 5V pin voltage, 5.6978 is calibrated voltage divider factor
    voltage = (((double)sum / (double)NUM_SAMPLES * 4.996) / 1024.0) * 5.69;
    return voltage;
}

void checkVoltageAndAct(Task* me) {
    double voltage = measureVoltage();

    if (overrideState == PENDING) {
        if (voltage >= TRIGGER_VOLTAGE) {
            overrideState = FORCEDOFF;
        } else {
            overrideState = FORCEDON;
        }
    }

    // To prevent relay loop, we stay LOW and ignore all overrides
    // if we are not on battery, which cuts off at 18V
    if (voltage < 16) {
        setState(LOW);
        overrideState = NORMAL;
        return;
    }

    if (overrideState == FORCEDON) {
        setState(HIGH);
    } else if (overrideState == FORCEDOFF) {
        setState(LOW);
    } else if (voltage >= TRIGGER_VOLTAGE) {
        setState(HIGH);
    } else {
        if (millis() - lastTriggerSwitch >= TRIGGER_DELAY) {
            lastTriggerSwitch = millis();
            setState(LOW);
        }
    }

    //Serial.println(voltage);
    //delay(100);
}

void printVoltage(Task *me) {
    double voltage = measureVoltage();

    if (voltage < 16) { // To prevent relay loop, we stay LOW and ignore all overrides
        setState(LOW);  // if we are not on battery, which cuts off at 18V
        overrideState = NORMAL;
    }

    Serial.println(voltage);
    delay(10);
}

Task timerAct(6000, checkVoltageAndAct);
Task timerOutput(1000, printVoltage);

void setup()
{
    bitSet(DIDR0, ADC0D);  // disable digital buffer on A0
    bitSet(DIDR0, ADC1D);  // disable digital buffer on A1
    bitSet(DIDR0, ADC2D);  // disable digital buffer on A2
    bitSet(DIDR0, ADC3D);  // disable digital buffer on A3
    bitSet(DIDR0, ADC4D);  // disable digital buffer on A4
    bitSet(DIDR0, ADC5D);  // disable digital buffer on A5
    pinMode(3, OUTPUT);
    digitalWrite(3, LOW);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    pinMode(12, INPUT_PULLUP); // override button
    enableInterrupt(12, overridePressed, FALLING);
    SoftTimer.add(&timerAct);
    SoftTimer.add(&timerOutput);
    Serial.begin(115200);
}

void loop() {
    SoftTimer.run();
    LowPower.powerStandby(SLEEP_1S, ADC_OFF, BOD_ON);
}
