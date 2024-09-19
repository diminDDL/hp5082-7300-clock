#include <Arduino.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <EEPROM.h>

#define DIGIT_PIN_1 D1 
#define DIGIT_PIN_2 D2
#define DIGIT_PIN_4 D3
#define DIGIT_PIN_8 D4

#define DIGIT_0_EN_PIN D8
#define DIGIT_1_EN_PIN D7
#define DIGIT_2_EN_PIN D6
#define DIGIT_3_EN_PIN D5

#define DOT_PIN D0

#define DST_REGION DST_EUROPE

// settings struct
struct Settings {
    int8_t timezoneOffset;
    bool daylightSaving;
    char ntpServer[128];
} settings;

// stuct to hold the current display state
struct Display {
    uint8_t digits[4];
    uint8_t selectedDigit;
    bool dot;
} dispState;

// helper function to select the digit to display
void selectDigit(uint8_t digit);

// helper function to set the value of the 4-bit display
void setVal(uint8_t val);

// set a value to a specific digit
void setDigit(uint8_t digit, uint8_t val);

// clear all digits
void clearAll();

// set the dot state
void writeDot(bool state);

// write the time to the display
void writeTime(uint8_t hour, uint8_t minute);

// initialize the pins
void initPins();

// check if daylight saving time is active
bool checkDST(unsigned long epoch, uint region = 0, int8_t timezone = 0);

// wifi manager
WiFiManager wm;
// custom fields
WiFiManagerParameter tz_field;
WiFiManagerParameter dst_field;
WiFiManagerParameter ntp_server_field;
// ntp client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// callback for the save settings event
void saveParamCallback();

// reset the ESP
void reset();

void setup() {
    WiFi.mode(WIFI_STA);

    Serial.begin(115200);

    EEPROM.begin(1024);

    uint8_t check = 0;
    check = EEPROM.read(512);

    Serial.println("EEPROM check:");
    Serial.println(check);

    if(check == 0xFF){
        Serial.println("EEPROM is empty, writing default settings");
        settings.timezoneOffset = 2;
        settings.daylightSaving = true;
        strcpy(settings.ntpServer, "pool.ntp.org");
        EEPROM.put(256, settings);
        EEPROM.write(512, 0);
        EEPROM.commit();
        reset();
    }else{
        EEPROM.get(256, settings);
        Serial.println("EEPROM is not empty, reading settings");
        Serial.println(settings.timezoneOffset);
        Serial.println(settings.daylightSaving);
        Serial.println(settings.ntpServer);
    }

    initPins();
    setDigit(0, 13);
    setDigit(1, 13);
    setDigit(2, 13);
    setDigit(3, 13);

    // Can be used to reset settings for debugging
    // wm.resetSettings();

    wm.setConfigPortalBlocking(false);
    wm.setConnectTimeout(15);

    // timezone offset
    const char* tz_input_str = "<br/><label for='tzfield'>Timezone Offset:</label><br><input type='number' name='tzfield' value='0' min='-12' max='14'>";
    new (&tz_field) WiFiManagerParameter(tz_input_str);

    // daylight saving time
    const char* dst_checkbox_str = "<br/><label for='dstfield'>Daylight Saving Time:</label><br><input type='radio' name='dstfield' value='1' checked> ON<br><input type='radio' name='dstfield' value='0'> OFF<br>";
    new (&dst_field) WiFiManagerParameter(dst_checkbox_str);

    // ntp server
    const char* ntp_server_input_str = "<br/><label for='ntpserverfield'>NTP Server:</label><br><input type='text' name='ntpserverfield' value='pool.ntp.org'>";
    new (&ntp_server_field) WiFiManagerParameter(ntp_server_input_str);

    wm.addParameter(&tz_field);
    wm.addParameter(&dst_field);
    wm.addParameter(&ntp_server_field);
    wm.setSaveParamsCallback(saveParamCallback);
    
    // set the order of the menu items
    // std::vector<const char *> menu = {"wifi","param","sep","info","restart","exit"};
    std::vector<const char *> menu = {"wifi","param","sep","exit"};
    wm.setMenu(menu);

    // set dark theme
    wm.setClass("invert");

    wm.setPreOtaUpdateCallback([](){
        Serial.println("Dummy OTA callback to prevent OTA updates. Resetting...");
        reset();
    });

    // configure the NTP client
    timeClient.begin();
    timeClient.setUpdateInterval(600000);    // once every 10 minutes

    // set the timezone offset and NTP server
    timeClient.setTimeOffset(3600 * settings.timezoneOffset);
    timeClient.setPoolServerName(settings.ntpServer);

    // try to connect to wifi
    if(wm.autoConnect("LED Clock Config")) {
        timeClient.update();
    }
}

// helper function to read parameter from server, for customhmtl input
String getParam(String name){
  String value;
  if(wm.server->hasArg(name)) {
    value = wm.server->arg(name);
  }
  return value;
}

void saveParamCallback(){
    Serial.println("[CALLBACK] saveParamCallback fired");
    Serial.println("PARAM tzfield = " + getParam("tzfield"));
    Serial.println("PARAM dstfield = " + getParam("dstfield"));
    Serial.println("PARAM showdatefield = " + getParam("showdatefield"));
    Serial.println("PARAM ntpserverfield = " + getParam("ntpserverfield"));

    settings.timezoneOffset = getParam("tzfield").toInt();
    settings.daylightSaving = (bool)getParam("dstfield").toInt();
    strcpy(settings.ntpServer, getParam("ntpserverfield").c_str());

    // save settings to EEPROM
    EEPROM.put(256, settings);
    EEPROM.write(512, 0);
    EEPROM.commit();

    timeClient.setTimeOffset(3600 * settings.timezoneOffset);
    timeClient.setPoolServerName(settings.ntpServer);
}

bool canUpdate = false;
bool firstStart = true;
bool forceUpdate = false;
bool dst = false;
unsigned long lastMillis = 0;

uint16_t adc_counter = 0;
uint8_t digit = 0;

int8_t hour = 0;
int8_t minute = 0;
int8_t second = 0;
int8_t lastSecond = 0;

int8_t testHour = 0;
int8_t testMinute = 0;
int8_t testSecond = 0;

void loop() {
    wm.process();

    // check if we are connected to wifi
    if(WiFi.status() == WL_CONNECTED && !canUpdate) {
        canUpdate = true;
    }

    // we don't need to update more often than once per second
    if(canUpdate && millis() - lastMillis > 1000) {

        if(timeClient.getEpochTime() == 0){
            forceUpdate = true;
        }

        lastMillis = millis();
        if(forceUpdate){
            timeClient.forceUpdate();
            forceUpdate = false;
            firstStart = true;
        }else{
            timeClient.update();
        }

        if(settings.daylightSaving){
            dst = checkDST(timeClient.getEpochTime(), settings.timezoneOffset);
            timeClient.setTimeOffset(3600 * (settings.timezoneOffset + dst));
            timeClient.update();
        }

        if(timeClient.isTimeSet()){
            testHour = timeClient.getHours();
            testMinute = timeClient.getMinutes();
            testSecond = timeClient.getSeconds();
        }
    }

    // checks for the time to be consistent (common issue with NTP)
    if(timeClient.isTimeSet() &&
    (((testHour - hour == 1 || testHour - hour == 0 || testHour - hour == -23) &&
    (testMinute - minute == 1 || testMinute - minute == 0 || testMinute - minute == -59)) || 
    firstStart)) {
        hour = testHour;
        minute = testMinute;
        second = testSecond;
        firstStart = false;
        forceUpdate = false;
    }else if(timeClient.isTimeSet() && !firstStart){    // if the time range constraints failed we force an NTP update
        // this logic is here because sometimes NTP returns all zeroes even when timeClient.isTimeSet() reports true
        forceUpdate = true;
    }

    // write the time to the display
    if(canUpdate && adc_counter == 0){
        writeTime(hour, minute);
    }else if(adc_counter == 0){
        writeDot(true);
        setDigit(0, 13);
        setDigit(1, 13);
        setDigit(2, 13);
        setDigit(3, 13);
    }

    // the second dot
    if(second != lastSecond) {
        writeDot(second % 2);
        lastSecond = second;
    }

    // the reset button functionality
    while(analogRead(A0) <= 50){
        adc_counter++;
        if(adc_counter % 8 == 0){
            digit++;
            if(digit > 3){
                digit = 0;
            }
        }

        for(uint8_t i = 0; i < 4; i++){
            if(i == digit){
                setDigit(i, 13);
            }else{
                setDigit(i, 12);
            }
        }
        

        // reset the ESP if the button is pressed for more than ~5 seconds
        if (adc_counter > 50) {
            adc_counter = 0;
            reset();
        }
        delay(100);
    }
    adc_counter = 0;
    
    // rate limit the loop
    delay(100);
}


void selectDigit(uint8_t digit) {
    digitalWrite(DIGIT_0_EN_PIN, digit != 0);
    digitalWrite(DIGIT_1_EN_PIN, digit != 1);
    digitalWrite(DIGIT_2_EN_PIN, digit != 2);
    digitalWrite(DIGIT_3_EN_PIN, digit != 3);
    dispState.selectedDigit = digit;
}

void setVal(uint8_t val) {
    digitalWrite(DIGIT_PIN_1, val & 0x01);
    digitalWrite(DIGIT_PIN_2, val & 0x02);
    digitalWrite(DIGIT_PIN_4, val & 0x04);
    digitalWrite(DIGIT_PIN_8, val & 0x08);
    dispState.digits[dispState.selectedDigit] = val;
}

void setDigit(uint8_t digit, uint8_t val) {
    selectDigit(digit);
    setVal(val);
}

void clearAll(){
    digitalWrite(DIGIT_0_EN_PIN, LOW);
    digitalWrite(DIGIT_1_EN_PIN, LOW);
    digitalWrite(DIGIT_2_EN_PIN, LOW);
    digitalWrite(DIGIT_3_EN_PIN, LOW);
    digitalWrite(DIGIT_PIN_1, HIGH);
    digitalWrite(DIGIT_PIN_2, HIGH);
    digitalWrite(DIGIT_PIN_4, HIGH);
    digitalWrite(DIGIT_PIN_8, HIGH);
    digitalWrite(DOT_PIN, HIGH);
    digitalWrite(DIGIT_0_EN_PIN, HIGH);
    digitalWrite(DIGIT_1_EN_PIN, HIGH);
    digitalWrite(DIGIT_2_EN_PIN, HIGH);
    digitalWrite(DIGIT_3_EN_PIN, HIGH);
    for (int i = 0; i < 4; i++) {
        dispState.digits[i] = 0;
    }
    dispState.dot = false;
}

void writeDot(bool state) {
    setDigit(1, dispState.digits[1]);   // the dot is at digit 1
    digitalWrite(DOT_PIN, !state);
    dispState.dot = !state;
    setDigit(dispState.selectedDigit, dispState.digits[dispState.selectedDigit]);   // restore the selected digit
}

void writeTime(uint8_t hour, uint8_t minute) {
    setDigit(0, hour / 10);
    setDigit(1, hour % 10);
    setDigit(2, minute / 10);
    setDigit(3, minute % 10);
}

void initPins(){
    pinMode(DIGIT_PIN_1, OUTPUT);
    pinMode(DIGIT_PIN_2, OUTPUT);
    pinMode(DIGIT_PIN_4, OUTPUT);
    pinMode(DIGIT_PIN_8, OUTPUT);
    pinMode(DOT_PIN, OUTPUT);
    pinMode(DIGIT_0_EN_PIN, OUTPUT);
    pinMode(DIGIT_1_EN_PIN, OUTPUT);
    pinMode(DIGIT_2_EN_PIN, OUTPUT);
    pinMode(DIGIT_3_EN_PIN, OUTPUT);
}

void reset(){
    wm.resetSettings();
    delay(100);
    ESP.restart();
}

enum DST_REGIONS {
    DST_EUROPE = 0,
    DST_NORTH_AMERICA = 1,
    DST_AUSTRALIA = 2,
    DST_SOUTH_AMERICA = 3
};

// Helper function to calculate the day of the week for a given epoch
// 0 = Sunday, 1 = Monday, ..., 6 = Saturday
int dayOfWeek(unsigned long epoch) {
    return (epoch / 86400L + 4) % 7;
}

// Helper function to calculate the number of days since January 1st, 1970
unsigned long daysSinceEpoch(unsigned long epoch) {
    return epoch / 86400L;
}

// Helper function to get the current month from the epoch
int getMonth(unsigned long epoch, int8_t timezone) {
    unsigned long days = daysSinceEpoch(epoch + timezone * 3600);
    int year = 1970;
    int month = 0;
    while (days >= 365) {
        days -= (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 366 : 365;
        year++;
    }
    static const int daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    for (month = 0; month < 12; month++) {
        int dim = daysInMonth[month];
        if (month == 1 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) {
            dim = 29;
        }
        if (days < dim) break;
        days -= dim;
    }
    return month + 1;
}

// Helper function to get the current day of the month
int getDayOfMonth(unsigned long epoch, int8_t timezone) {
    unsigned long days = daysSinceEpoch(epoch + timezone * 3600);
    int year = 1970;
    int month = 0;
    while (days >= 365) {
        days -= (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 366 : 365;
        year++;
    }
    static const int daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    for (month = 0; month < 12; month++) {
        int dim = daysInMonth[month];
        if (month == 1 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) {
            dim = 29;
        }
        if (days < dim) break;
        days -= dim;
    }
    return days + 1;
}

// Main function to check if DST is active
bool checkDST(unsigned long epoch, uint region, int8_t timezone) {
    int month = getMonth(epoch, timezone);
    int day = getDayOfMonth(epoch, timezone);
    int dow = dayOfWeek(epoch);

    switch (region) {
        case DST_EUROPE:
            // DST in Europe: Last Sunday of March to Last Sunday of October
            if ((month > 3 && month < 10) || 
                (month == 3 && (day - dow) >= 25) || 
                (month == 10 && (day - dow) <= 24)) {
                return true;
            }
            return false;

        case DST_NORTH_AMERICA:
            // DST in North America: Second Sunday of March to First Sunday of November
            if ((month > 3 && month < 11) || 
                (month == 3 && (day - dow) >= 8) || 
                (month == 11 && (day - dow) <= 0)) {
                return true;
            }
            return false;

        case DST_AUSTRALIA:
            // DST in Australia: First Sunday of October to First Sunday of April
            if ((month > 10 || month < 4) || 
                (month == 10 && (day - dow) >= 1) || 
                (month == 4 && (day - dow) <= 0)) {
                return true;
            }
            return false;

        case DST_SOUTH_AMERICA:
            // DST in Chile (example for South America): First Sunday of September to First Sunday of April
            if ((month > 9 || month < 4) || 
                (month == 9 && (day - dow) >= 1) || 
                (month == 4 && (day - dow) <= 0)) {
                return true;
            }
            return false;

        default:
            return false;
    }
}