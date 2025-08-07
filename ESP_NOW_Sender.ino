 /*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp8266-esp-now-wi-fi-web-server/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

/*Build 2025-06-10    -- Voltage divider was a nice idea but I don't know what the input voltage will be
**                   -- changed to monitoring 3.3 line if it goes below max then it goes from 'good' to 'OK' if it is 15% less report 'Bad'
**       2025-07-01  -- Changed IO to match home made PC Board layout
**       2025-07-21  -- new PC Board Design

  *  Board version 1.7 Changes: 08/02/2025
  * 1. Battery Voltage compare is 2K : 2K for 3V input which cuts the voltage in half
  * so by calculating the percentage of A0 against 1024
  * I can determine the fill voltage of the battery
  * see readBattery()

 2025-08-07
 USSUES:
  1. Jumper to read battery voltage causes MCP1700 to get super hot on battery power
  2. Deep sleep causes LED_FAILURE to go to an ON state
    Both require hardware changes
 */

#include <espnow.h>
#include <ESP8266WiFi.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
/*   GPIO - GPIO  Pin   PIN   GPIO
 *          RST         TX    1     OUTPUT
 *INPUT     ADC   A0    RX    3     INPUT
 *I/O       16    D0    D1    5     I/O
 *I/O       14    D5    D2    4     I/O
 *I/O       12    D6    D3    0     OUTPUT
 *I/O       13    D7    D4    2     OUTPUT
 *OUTPUT    15    D8    G     GND
 *          3.3         5V    5V
 */
char BornOn[21] = "  Build   2025-08-02";
// Set your Board ID (ESP32 Sender #1 = BOARD_ID 1, ESP32 Sender #2 = BOARD_ID 2, etc)
#define BOARD_ID 1 //Garden board 2 is garage, board 1 mailbox
#define DHTPIN 13     // 7 Digital pin connected to the DHT sensor
#define DHTPWR 15     // D8  GPIO12 POWER TO TEMP SENSOR
#define LED_FAILURE 0  // D3

#define PIR 12          // D6 GPIO13
#define TESTPin 14      // D5 GPIO 14 Jumper low for test
#define DHTTYPE    DHT22     // DHT 22 (AM2302)

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// setup screen position definations
#define line1 0
#define line2 12
#define line3 24
#define line4 36
#define line5 48  // line 4 at font size of 2
#define line6 60
#define MiddleScreen 48

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
char buffer[128];// = "  Build   2025-07-29";   // build output strings here
DHT dht(DHTPIN, DHTTYPE);
unsigned long downTime = 895e6;    //15 minutes - 5 seconds; 15 seconds = 15e6;
//MAC Address of the receiver 
uint8_t broadcastAddress[] = {0xE8, 0xDB, 0x84, 0xC5, 0xA6, 0xFF};
//uint8_t broadcastAddress[] = {0xE8, 0xDB, 0x84, 0xC5, 0xA6, 0xF0};// bad address

//Structure example to send data
//Must match the receiver structure
typedef struct struct_message {
    int id;
    float temp;
    float hum;
    float bat;
    int readingId;
} struct_message;

//Create a struct_message called myData
struct_message myData;

bool displayOK = false;
char dataSent[8];

constexpr char WIFI_SSID[] = "SLAN";
/*************************************
 * 
 *  Done with all the definitions 
 * 
 * 
 *************************************/

void displayWrite(int x, int y,int size,String msg,int clear = 1) 
  {
  if (displayOK) {
    if (clear == 1)
      display.clearDisplay();
    display.setTextSize(size);
    display.setCursor(x, y);
    display.print(msg);
    display.display();
    }
  }
  
int32_t getWiFiChannel(const char *ssid) {
  if (int32_t n = WiFi.scanNetworks()) {
    for (uint8_t i=0; i<n; i++) {
      if (!strcmp(ssid, WiFi.SSID(i).c_str())) {
        return WiFi.channel(i);
        }
      }
    }
  return 0;
  }

float readTemperature() {
  float t = dht.readTemperature(true); //convert to F
  if ( t = NAN)
    {
      delay(3000);
      t = dht.readTemperature(true); 
    }
  return t;
  }

float readHumidity() {
  float h = dht.readHumidity();
  return h;
  }

float readBattery() {
  /*
   * voltage is divided by 2
   * take the percentage of the reading * 2 > 1024
   * add 1 to that result and multiply it by 3.2 
   * and you have the input voltage
   */
  int x = analogRead(0);
  float fullv = 2.0 * float(x);
  float diff = fullv - 1024;
  float pct = diff / 1024.0 + 1.0;
  return(3.20 * pct);
}

void printBattery() {
  // this is reading 3.3volts would be 100 so reading 82 is a percentage
  Serial.print(F("Battery: "));
  float x = readBattery();
  Serial.print(readBattery());
  Serial.println(F(" volts"));
}

// Callback when data is sent
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("Last Packet Send Status: Delivery ");
  if (sendStatus == 0){
    sprintf(dataSent,"success");
    digitalWrite(LED_FAILURE, LOW);
    digitalWrite(LED_BUILTIN,LOW);  // turn it on
  }
  else{
    sprintf(dataSent," failed");
    digitalWrite(LED_FAILURE, HIGH);
    digitalWrite(LED_BUILTIN,HIGH);  // turn it off
  }
    Serial.println(dataSent);
}

 void doStuff(){
     digitalWrite(DHTPWR, HIGH); // Turn on support devices
     printBattery();
     Serial.println(F("delay 2 seconds."));
     Serial.println(BornOn);
     displayWrite(0,1,2,BornOn,1);

     delay(2000);
        
      myData.id = BOARD_ID;
      myData.temp = readTemperature();
      myData.hum = readHumidity();
      myData.bat = readBattery();
      myData.readingId = 0;
    
      Serial.print("\nTemp ");
      Serial.println(myData.temp);  
      Serial.print(F("Humidity "));
      Serial.println(myData.hum);
    
      Serial.print(F("Board "));
      Serial.println(BOARD_ID);
            
      esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
      // 10 characters in a line, 4 lines at text size 2
      if (displayOK){
          sprintf(buffer,"Batt %2.2f Temp  %2.0f  Humidit %2.0f",myData.bat, myData.temp,myData.hum);
          displayWrite(0,1,2,buffer);
      
          display.setCursor(0,line5);  // line 5 on the display
          display.print("Xmt");
          display.setCursor(40,line5);
          display.print(dataSent);
          display.display();
          delay(4000);
           }
         else
          Serial.println(F("Problem writing to display."));
    
      digitalWrite(DHTPWR,LOW); // Hopefully DHT is low now
      // leave LEDs as OnDataSent set them
//      digitalWrite(LED_FAILURE, HIGH);
//      digitalWrite(LED_BUILTIN, HIGH);   // Turn it off
//      Serial.println(F("Built-in LED off, Red LED off"));
      display.clearDisplay();
      display.display();
      Serial.println(F("Done doing stuff."));
      delay(1000);      
 }  // done doing stuff

void displayA0()
  {
    int X = analogRead(0);
    sprintf(buffer,"A0 %2d",X);
    displayWrite(0,1,2,buffer,1);
  }
  
void setup() {
  //Init Serial Monitor
  Serial.begin(115200);
  Serial.println("");
  pinMode(DHTPWR, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_FAILURE, OUTPUT);
  pinMode(TESTPin, INPUT);
  
//  digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (Reverse logic)
  digitalWrite(LED_FAILURE, LOW);  
  digitalWrite(DHTPWR, HIGH); // Turn on support devices
  dht.begin();

  if(display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) 
    displayOK = true;
  else
    displayOK = false;

  if (displayOK) {
    display.clearDisplay();
    Serial.println("\nDisplay connected.");
    display.setTextColor(WHITE);
    delay(500);
    }

// Set device as a Wi-Fi Station and set channel
  WiFi.mode(WIFI_STA);

  int32_t channel = getWiFiChannel(WIFI_SSID);

  WiFi.printDiag(Serial); // Uncomment to verify channel number before
  wifi_promiscuous_enable(1);
  wifi_set_channel(channel);
  wifi_promiscuous_enable(0);
  WiFi.printDiag(Serial); // Uncomment to verify channel change after

  // Init ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println(F("Error initializing ESP-NOW"));
    displayWrite(0,1,2,"Error initializing ESP-NOW");
    return;
    }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
   esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);

  esp_now_register_send_cb(OnDataSent);
  
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
  
  doStuff();
    Serial.println(buffer);

     if (digitalRead(TESTPin) == LOW)
        Serial.println(F("TESTPin is Low, In TEST MODE."));
     else
        {
        Serial.println(F("TESTPin is High, Deep Sleep Enabled."));
        Serial.println(F("Shutting down now, but I'll be back."));
//        delay(5000);  // when does that LED turn on
        Serial.println(F("set it off"));
        digitalWrite(LED_FAILURE, LOW);  
        delay(1000);  // you can see it go off for 1 second then back on
        Serial.println(F("setting FAILURE_LED to input"));
        pinMode(LED_FAILURE, INPUT);
        ESP.deepSleepInstant(downTime);          
        }
}
  /********************* End of Setup *************/

void loop() 
  {

    displayA0();
    // you get here if jumper is on test pin
    //  digitalWrite(DHTPWR, LOW); // Turn off support devices
    delay(5000);  //wait a bit
    doStuff();    // send status again
  }
