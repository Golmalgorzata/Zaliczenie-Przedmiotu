#include <Arduino.h>
#include <chrono>
#include <driver/adc.h>
#include "splitString.h"

// define I/O
#define iLED 2   // internal LED (blue)
#define bLED 13  // blue LED
#define adc 34   // ADC pin after soldering

#define samplesMaxNo 19999 // max number of samples (Watch!!! RAM limitation)

// zmienne globalne
int samples[samplesMaxNo]; // rezerwacja pamieci na [20]k sampli  [0]time [1]led status [2]intensity // zajmą 40kb*3 sample + czas w formacie ms + info led
int samplesCount = 0;      // current sample number
int samplesNo = 0;         // number of samples to measure

int samplesBE; // number of samples before excitation
int samplesUE; // number of samples under excitation
int samplesAE; // number of samples after excitatnion

int samplesRE = 0; // reference singal (dark singnal/current)

hw_timer_t * timerADC = NULL;  // reference to hardware 80Mhz clock

unsigned long timeB;
unsigned long timeE;


/**
 * @brief Obsługuje przerwania timera do odczytu wartości z przetwornika analogowo-cyfrowego (ADC).
 * 
 * Argumenty:
 * - Funkcja nie wymaga argumentów.
 * 
 * Return:
 * - Funkcja nie zwraca wartości.
 */
void IRAM_ATTR onTimeReadADC(){
    if(samplesCount==0){timeB = micros();}
    samples[samplesCount]=analogRead(adc);  // analog read procedure based on interrupt system
    samplesCount++;

    if(samplesCount>samplesBE && samplesCount<(samplesBE+samplesUE)){
            digitalWrite(iLED, HIGH);
            digitalWrite(bLED, LOW);
        }
    else{
            digitalWrite(iLED, LOW);
            digitalWrite(bLED, HIGH);
        }

    if(samplesCount==samplesNo){
        timeE = micros();
        // stop timers and interrupts
        timerAlarmDisable(timerADC);  
        timerDetachInterrupt(timerADC);
        
        // reset sample counter
        samplesCount=0;
        }
}

/**
 * @brief Wyświetla próbki w konsoli w formacie czas-intensity.
 * 
 * Argumenty:
 * - `int iMAX`: liczba próbek do wydrukowania.
 * - `int* sampl`: wskaźnik na tablicę z próbkami.
 * 
 * Return:
 * - Funkcja nie zwraca wartości.
 */
void printer(int iMAX, int* sampl ){       
    for(int i=0; i<iMAX ; i++) {
        Serial.printf("time: %d - %d\n", time[i], samples[i]);
    
    }
}

/*
// millis() overflow confirmation: reset micros
noInterrupts();
timer0_millis = 4294901760;
interrupts();
*/

/**
 * @brief Inicjalizuje ustawienia początkowe dla Arduino, takie jak porty I/O, seriowy, ADC itp.
 * 
 * Argumenty:
 * - Funkcja nie wymaga argumentów.
 * 
 * Return:
 * - Funkcja nie zwraca wartości.
 */
void setup() {
    Serial.begin(2000000);
    delay(500);
    Serial.println("READY:");
    Serial.println("Command list:");
    Serial.println("MF - measure fluoroscence BE[ms] UE[ms] AE[ms]");
    Serial.println("CM - continious measurment from ADC");
    Serial.println("SD - show data (CSV style)");
    
    timerADC = timerBegin(0, 80, true); // zegar 80MHz dzielimy na 80 -> 1MHz podstawa do obliczania przerwań

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_6);

    analogSetPinAttenuation(adc, ADC_6db);
    adcAttachPin(adc);
    
    pinMode(iLED, OUTPUT); // wewn dioda LED
    pinMode(bLED, OUTPUT); // zewn dioda LED

    digitalWrite(bLED, HIGH); // turn OFF excitation LIGHT
}

/**
 * @brief Główna pętla programu Arduino, obsługująca komendy z portu szeregowego.
 * 
 * Argumenty:
 * - Funkcja nie wymaga argumentów.
 * 
 * Return:
 * - Funkcja nie zwraca wartości.
 */
void loop() {
    if (Serial.available() > 0) {
        String command = Serial.readStringUntil('\n');

        if(command.startsWith("MF")){
            samplesRE=0;
            /* procedura zerowania sygnału tła
            for(int i=0;i<100;i++){
                samplesRE=samplesRE+analogRead(adc);
                delay(1);
            }
            samplesRE=samplesRE/100;
            */
            
            // Measure OJIP fluorescence
            samplesBE = splitString(command, ' ', 1).toInt();
            samplesUE = splitString(command, ' ', 2).toInt();
            samplesAE = splitString(command, ' ', 3).toInt();

            samplesNo = samplesBE + samplesUE + samplesAE;

            Serial.printf("Time before excitation BE: %d [samples]\n", samplesBE); 
            Serial.printf("Time under excitation  UE: %d [samples]\n", samplesUE); 
            Serial.printf("Time after excitation  AE: %d [samples]\n", samplesAE); 

            // start measure
            if(samplesNo <= samplesMaxNo){
                // clear samples array
                for(int i = 0; i < samplesMaxNo; i++){
                    samples[i] = 0;
                }

                // turning on interrupts to start measuring
                timerAttachInterrupt(timerADC, &onTimeReadADC, true);    // wywołujemy onTime
                timerAlarmWrite(timerADC, 100, true); // 1MHz/1 000 000 = 1 sekunda /1000 to 1000 razy na sekundę 
                timerAlarmEnable(timerADC); 
        
                // blocking main loop due to measurement
                delay(round(samplesNo / 10 + 100));

                for(int i = 0; i < samplesNo; i++) {
                    Serial.printf("%d\n", samples[i] - samplesRE + 1);
                }
            }
            else {
                Serial.printf("ERROR: to long measure time.\nMax measure time = %d [ms].\nEntered time %d[ms]\n", samplesMaxNo, samplesNo);
            }
        }
        else if(command.startsWith("CM")){
            // Calculate parameters and print to serial out
            Serial.println("Continius measurement -> serial plot");
            digitalWrite(iLED, HIGH);
            for(;;){
                Serial.println(analogRead(adc));
                delay(1);
                if(Serial.available() > 0){
                    digitalWrite(iLED, LOW);
                    break;
                }
            }
        }
        else if(command.startsWith("SD")){
            // Print out raw data
            Serial.println("microSecond: ");
            Serial.println(timeE - timeB);
            
            Serial.println("Show sample data as CSV");

            for(int i = 0; i < samplesNo; i++) {
                Serial.printf("%d\n", samples[i] - samplesRE + 1);
            }
        }
        else if(command.startsWith("ON")){
            digitalWrite(iLED, HIGH);
            digitalWrite(bLED, LOW);
        }
        else if(command.startsWith("OFF")){
            digitalWrite(iLED, LOW);
            digitalWrite(bLED, HIGH);
        }     
        else if(command.startsWith("RE")){
            samplesRE = 0;
            for(int i = 0; i < 100; i++){
                samplesRE += analogRead(adc);
                delay(1);
            }
            samplesRE = samplesRE / 100;
        }     
        else{
            Serial.println("ERROR: Unknown command!");
        }
    }
}
