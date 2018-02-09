/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 by Daniel Eichhorn
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "espressif_logo.h"
#include "battery_icon.h"
//#include <BLEAdvertisedDevice.h>
// Include the correct display library
// For a connection via I2C using Wire include
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306.h" // alias for `#include "SSD1306Wire.h"`

// BLE UUIDs
#define GENERIC_DISPLAY 320
static const BLEUUID ALERT_DISPLAY_SERVICE_UUID = BLEUUID("3db02924-b2a6-4d47-be1f-0f90ad62a048");
static const BLEUUID DISPLAY_MESSAGE_CHARACTERISTIC_UUID = BLEUUID("8d8218b6-97bc-4527-a8db-13094ac06b1d");
static const BLEUUID DISPLAY_TIME_CHARACTERISTIC_UUID = BLEUUID("b7b0a14b-3e94-488f-b262-5d584a1ef9e1");
static const BLEUUID DISPLAY_DISPLAY_ORIENTATION_CHARACTERISTIC_UUID = BLEUUID("0070b87e-d825-43f5-be0c-7d86f75e4900");

#define BUFF_LEN 256


volatile bool connected = false; //0=advertising; 1=connected
volatile bool hasText = false, hasBattText = false;
volatile bool hasTimeText = false;
volatile bool hasLongText = false;
volatile int yScrollPos = 0;
char* payload = new char[BUFF_LEN];
char* time_buffer = new char[BUFF_LEN];
volatile int batt_level = 0; // 
volatile int display_orientation = 0; //0=none;1=flip

#define SCROLL_DELAY 6
#define MAX_SCROLL_WINDOW_HEIGHT 75

// Initialize the OLED display using Wire library
SSD1306  display(0x3c, 5, 4);


class DisplayOrientationCharacteristicCallback: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    std::string str = pCharacteristic->getValue(); //read values
    display_orientation = (int)str[0];
    Serial.println(" DisplayOrientationCharacteristicCallback->onWrite()");
  }
  
};
/**
 * Callback to handle Time Charcterisitc events
 */
class TimeCharacteristicCallback: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pTimeCharacteristic) {
    std::string str = pTimeCharacteristic->getValue(); //read values
    memset(time_buffer, '\0', sizeof(time_buffer)); //clear buffer
    strcpy(time_buffer, &str.c_str()[1]); //first char indicates battery-level
    batt_level = (int)str[0]; //char to int

    
    char buff[16];
    sprintf(buff, "Batt: %d%%", str[0]);
    Serial.print(buff);
    Serial.println(" TimeCharacteristicCallback->onWrite()");

    hasLongText = false;
    hasText = false;
    hasTimeText = true;
  }
};
/**
 * Callback to handle Notification characteristic events
 */
class DisplayCharacteristicCallback: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    // write text to OLED
    std::string str = pCharacteristic->getValue();
    BLEUUID uuid = pCharacteristic->getUUID();
    std::string uuid_s = uuid.toString();
    Serial.print("onWrite() {uuid=");
    Serial.print(String(uuid_s.c_str()));
    Serial.print(",len=");
    Serial.print(str.length());
    Serial.println("}");

    memset(payload, '\0', sizeof(payload));
    strcpy(payload, str.c_str());
    yScrollPos = 0;

    hasLongText = str.length() > 40;
    hasText = true;
    hasTimeText = false;
  }
};

class MyServerCallback: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    //todo display `connected` on the screen
    Serial.println("LE onConnect");
    
    connected = true;
    memset(payload, 0x00, BUFF_LEN);
    hasLongText = false;
    hasText = false;
    hasTimeText = false;
    yScrollPos = 0;
  }

  void onDisconnect(BLEServer* pServer) {
    // todo display `disconnected` on the screen
    Serial.println("LE onDisconnect");
    connected = false;
  }
};

void setupBLEServer() {
  //init BLE server
  BLEDevice::init("VESPA-32");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallback());
  // Alert Display Service
  BLEService *pService = pServer->createService( ALERT_DISPLAY_SERVICE_UUID );

//  BLECharacteristic* pCharacteristic = pService->createCharacteristic(
//                                          DISPLAY_MESSAGE_CHARACTERISTIC_UUID,
//                                          BLECharacteristic::PROPERTY_WRITE
//                                          );
//  pCharacteristic->setWriteProperty(true);

  BLECharacteristic* pCharacteristicText = new BLECharacteristic(DISPLAY_MESSAGE_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE_NR); //Request MTU=500 from client
  pCharacteristicText->setCallbacks(new DisplayCharacteristicCallback());
  pService->addCharacteristic(pCharacteristicText);

  // Display Time Characteristic
  BLECharacteristic* pCharacteristicTime = new BLECharacteristic(DISPLAY_TIME_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE_NR);
  pCharacteristicTime->setCallbacks(new TimeCharacteristicCallback());
  pService->addCharacteristic(pCharacteristicTime); 

  // Change Display Orientation
  BLECharacteristic* pCharacteristicDisplayOrientation = new BLECharacteristic(DISPLAY_DISPLAY_ORIENTATION_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE_NR);
  pCharacteristicDisplayOrientation->setCallbacks(new DisplayOrientationCharacteristicCallback());
  pService->addCharacteristic(pCharacteristicDisplayOrientation); 

  pService->start();
     
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.gap.appearance.xml
  pAdvertising->setAppearance(GENERIC_DISPLAY); //Generic Display
  pAdvertising->start();
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();
  Serial.println("hello");

  //init BLE server
  setupBLEServer();
  
  // Initialising the UI will init the display too.
  display.init();

//  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
}

//void drawFontFaceDemo() {
//    // Font Demo1
//    // create more fonts at http://oleddisplay.squix.ch/
//    display.setTextAlignment(TEXT_ALIGN_LEFT);
//    display.setFont(ArialMT_Plain_10);
//    display.drawString(0, 0, "Hello world 2");
//    display.setFont(ArialMT_Plain_16);
//    display.drawString(3, 10, "Hello world 3");
//    display.setFont(ArialMT_Plain_24);
//    display.drawString(6, 26, "Hello world 4");
//}


void oledWriteText(char* text, uint32_t y = 0) {
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  display.drawStringMaxWidth(0, y, 120, text);
}

void drawBatteryIcon() {
    // see http://blog.squix.org/2015/05/esp8266-nodemcu-how-to-create-xbm.html
    // on how to create xbm files
    display.drawXbm(0, 49, batt_icon_width, batt_icon_height, batt_icon_bits);
}

void oledWriteBattLevel(int batt_level) {
  char buff[16];
//  itoa(batt_level, buff, 10);
  drawBatteryIcon();
  sprintf(buff, "%d%%", batt_level);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  display.drawString(15, 48, buff);
}

void oledWriteTimeText(char* text, int batt_level) {
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_24);
  display.drawString(36, 10, text);
  //
  oledWriteBattLevel(batt_level);
}

void drawImageDemo() {
    // see http://blog.squix.org/2015/05/esp8266-nodemcu-how-to-create-xbm.html
    // on how to create xbm files
    display.drawXbm(35, 2, logo_width, logo_height, logo_bits);
}

void loop() {
    // clear the display
  display.clear();
  // put your main code here, to run repeatedly:
  if (connected) {
    if (hasText) {
      int yPos = 0;
      if (hasLongText) {
        yScrollPos = (yScrollPos + 1) % MAX_SCROLL_WINDOW_HEIGHT;
        if (yScrollPos >= SCROLL_DELAY) {
          // a small delay before applying the scroll-y position
          yPos = -(yScrollPos - SCROLL_DELAY);
        }
      }
      oledWriteText(payload, yPos);
    } else if (hasTimeText) {
      oledWriteTimeText(time_buffer, batt_level);
    } else if (display_orientation != 0) {
      display.flipScreenVertically();
      display_orientation = 0;
    } else {
      oledWriteText("Connected");
    }
  } else {
//    oledWriteText("Advertising...");
    drawImageDemo();
  }
  //
  
//  drawFontFaceDemo();
  // write the buffer to the display
  display.display();
  //
  delay(500); //500ms
}


