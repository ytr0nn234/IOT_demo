//Yurii Osypenko 
//Demo of smart station
//

#include <WiFi.h>
#include <GyverPortal.h>
#include <GyverButton.h>
#include <FastLED.h>
#include <Wire.h>
#include <BH1750.h>

//NETWORK SETTINGS
const char* ssid = "Set";      //SSID of network
const char* password = "12345678";  // passwordw

//Pins
#define PIN_MQ135    34
#define PIN_SDA      32
#define PIN_SCL      33
#define PIN_BUTTON   27
#define PIN_LEDS     14

//LED matrix setuo
#define NUM_LEDS      32 //leds
#define LED_TYPE      WS2812B
#define COLOR_ORDER   GRB
CRGB leds[NUM_LEDS];

#define AIR_THRESHOLD 1500 

//LIB's
GyverPortal portal;
GButton touchBtn(PIN_BUTTON, LOW_PULL, NORM_OPEN); 
BH1750 lightMeter;

//variables
int airValue = 0;
float luxValue = 0;

int currentMode = 1; //First mode
const char* modesList[] = {"OFF", "White", "Rainbow", "Fire", "Holiday", "Air Monitor"};

//Brightness
bool autoBrightness = true; 
int targetBrightness = 100;  
int currentBrightness = 100; 
int manualBrightness = 100; 

//timers impl.
uint32_t tmrSensors, tmrLeds, tmrSmooth; 

//Html simple interface
void build() {
  GP.BUILD_BEGIN();
  GP.THEME(GP_DARK);
  
  //Update
  GP.UPDATE("lbl_air,lbl_lux,lbl_status"); 
  
  GP.TITLE("Room Monitor"); //title
  GP.HR();

  GP.LABEL("SENSORS");
  GP.BLOCK_BEGIN();
    GP.LABEL("Air Quality (PPM):"); GP.LABEL_BLOCK("---", "lbl_air"); GP.BREAK();
    GP.LABEL("Light (Lux):");   GP.LABEL_BLOCK("---", "lbl_lux");
  GP.BLOCK_END();

  GP.HR();
  
  //Selector of modes
  GP.LABEL("EFFECTS");
  GP.SELECT("mode_sel", "OFF,White,Rainbow,Fire,Holiday,Air Monitor", currentMode);

  GP.HR();
  
  //Brightness bar and buttons
  GP.LABEL("BRIGHTNESS CONTROL");
  GP.LABEL_BLOCK("Mode: AUTO", "lbl_status"); 
  
  GP.BUTTON("btn_auto", "SET AUTO");
  GP.BUTTON("btn_manual", "SET MANUAL");
  
  GP.BREAK();
  GP.SLIDER("sld_bright", manualBrightness, 0, 255);
  
  GP.BUILD_END();
}

//Web system
void action() {
  //Data to browser
  if (portal.update()) {
    if (portal.update("lbl_air")) portal.answer(String(airValue));
    if (portal.update("lbl_lux")) portal.answer(String(luxValue, 0));
    
    //Statusupdate
    if (portal.update("lbl_status")) {
      if (autoBrightness) portal.answer("Mode: AUTO");
      else portal.answer("Mode: MANUAL");
    }
  }

  //Browser to server
  if (portal.click("mode_sel")) currentMode = portal.getInt("mode_sel");
  
  if (portal.click("btn_auto")) autoBrightness = true;
  
  if (portal.click("btn_manual")) autoBrightness = false;
  
  if (portal.click("sld_bright")) {
    manualBrightness = portal.getInt("sld_bright");
    autoBrightness = false; 
    targetBrightness = manualBrightness;
  }
}

void setup() {
  Serial.begin(115200); //serial staret
  Wire.begin(PIN_SDA, PIN_SCL);
  touchBtn.setDebounce(50); 
  
  if (lightMeter.begin()) Serial.println("BH1750 OK"); //check for modules
  else Serial.println("BH1750 ERROR");

  FastLED.addLeds<LED_TYPE, PIN_LEDS, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(100);
  FastLED.clear();
  FastLED.show();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi Connected!");
  Serial.println(WiFi.localIP());

  portal.attachBuild(build);
  portal.attach(action);
  portal.start();
}

void handleLEDs();

void loop() {
  portal.tick();
  touchBtn.tick();
  
  //Chanhe mode if button
  if (touchBtn.isClick()) {
    currentMode++;
    if (currentMode > 5) currentMode = 0; //mode
  }
  
  //Brightness and sensor
  if (millis() - tmrSensors >= 500) {
    tmrSensors = millis();
    
    int raw = analogRead(PIN_MQ135);
    airValue = map(raw, 0, 4095, 400, 5000); 

    if (lightMeter.measurementReady()) luxValue = lightMeter.readLightLevel();

    if (autoBrightness) {
      // Brightness loguic 
      int val = map((int)luxValue, 0, 500, 10, 255);
      val = constrain(val, 10, 255);
      
      if (abs(val - targetBrightness) > 10) {
        targetBrightness = val;
      }
    }
  }

  //smooth brightness
  if (millis() - tmrSmooth >= 20) {
    tmrSmooth = millis();
    if (currentBrightness != targetBrightness) {
      if (currentBrightness < targetBrightness) currentBrightness++;
      else currentBrightness--;
  
      FastLED.setBrightness(currentBrightness);
    }
  }

  //effects-
  if (millis() - tmrLeds >= 20) {
    tmrLeds = millis();
    handleLEDs(); 
    FastLED.show(); 
  }
}

//logic of effects
void handleLEDs() {
  static uint8_t hue = 0; 
  switch (currentMode) {
    case 0: // OFF
      fill_solid(leds, NUM_LEDS, CRGB::Black); 
      break;
      
    case 1: // Wrhite
      fill_solid(leds, NUM_LEDS, CRGB::White); 
      break;
      
    case 2: // Rainbow
      fill_rainbow(leds, NUM_LEDS, hue++, 7); 
      break;
      
    case 3: // Fire
      fadeToBlackBy(leds, NUM_LEDS, 20);
      if (random8() < 120) leds[random16(NUM_LEDS)] += CHSV(0 + random8(30), 255, 150 + random8(105));
      break;
      
    case 4: //new year
      fadeToBlackBy(leds, NUM_LEDS, 10);
      if (random8() < 40) {
         int r = random8(3);
         leds[random16(NUM_LEDS)] = (r==0)?CRGB::Red:(r==1)?CRGB::Green:CRGB::Gold;
      }
      break;
      
    case 5: { // air
      //400 clean2500 dirty
      //
      int mapHue = map(airValue, 400, 2500, 96, 0);
      fill_solid(leds, NUM_LEDS, CHSV(constrain(mapHue, 0, 96), 255, 255));
    } break; 
  }
}