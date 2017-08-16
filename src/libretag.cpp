#include <Arduino.h>
#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <TFT_Drivers/ST7735_Defines.h>
#include <SPI.h>
#include <i2s.h>
#include <i2s_reg.h>
#include "wavspiffs.h"
#include <painlessMesh.h>
#include <fastLED.h>
#include <ClickEncoder.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <stdint.h>
#include <algorithm>
#include "IRremoteESP8266.h"
#include "IRrecv.h"
#include "IRsend.h"
#include "IRutils.h"

#define OTA_SSID "TheMatrix"
#define OTA_PASS "W1reL3$$K3y"
#define OTA_HOST "www.fisk.me.uk"
#define OTA_PATH "/firmware.bin"

#define ENCODER_PINA     12
#define ENCODER_PINB     13
#define ENCODER_BTN      4
//#define ENC_DECODER ENC_FLAKY

#define ENCODER_STEPS_PER_NOTCH    4

IRsend irsend(16);
IRrecv irrecv(5);
decode_results results;
ClickEncoder encoder = ClickEncoder(ENCODER_PINA,ENCODER_PINB,ENCODER_BTN,ENCODER_STEPS_PER_NOTCH);
TFT_eSPI tft = TFT_eSPI();
#define TFT_W 160
#define TFT_H 128
int bullets=30;
byte hwRole;
painlessMesh  mesh;
#define NUM_LEDS 1
CRGB leds[NUM_LEDS];
static int16_t last, value, bulletdelay;
static uint32_t lastService = 0;
static uint32_t lastService2 = 0;

const unsigned int ICACHE_RODATA_ATTR fakePwm[]={
  0x00000010, 0x00000410, 0x00400410, 0x00400C10, 0x00500C10, 0x00D00C10, 0x20D00C10, 0x21D00C10,
  0x21D80C10, 0xA1D80C10, 0xA1D80D10, 0xA1D80D30, 0xA1DC0D30, 0xA1DC8D30, 0xB1DC8D30, 0xB9DC8D30,
  0xB9FC8D30, 0xBDFC8D30, 0xBDFE8D30, 0xBDFE8D32, 0xBDFE8D33, 0xBDFECD33, 0xFDFECD33, 0xFDFECD73,
  0xFDFEDD73, 0xFFFEDD73, 0xFFFEDD7B, 0xFFFEFD7B, 0xFFFFFD7B, 0xFFFFFDFB, 0xFFFFFFFB, 0xFFFFFFFF};

  #define   MESH_PREFIX     "whateverYouLike"
  #define   MESH_PASSWORD   "somethingSneaky"
  #define   MESH_PORT       5555


  void receivedCallback( uint32_t from, String &msg ) {
    Serial.printf("startHere: Received from %u msg=%s\n", from, msg.c_str());
  }

  void newConnectionCallback(uint32_t nodeId) {
      Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
  }

  void changedConnectionCallback() {
      Serial.printf("Changed connections %s\n",mesh.subConnectionJson().c_str());
  }

  void nodeTimeAdjustedCallback(int32_t offset) {
      Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),offset);
  }


// Non-blocking I2S write for left and right 16-bit PCM
bool ICACHE_FLASH_ATTR i2s_write_lr_nb(int16_t left, int16_t right){
  /*int sample = right & 0xFFFF;
  sample = sample << 16;
  sample |= left & 0xFFFF;*/
  static int err=0;
  int samp = left;
  samp=(samp+32768);  //to unsigned
  samp-=err;      //Add the error we made when rounding the previous sample (error diffusion)
  //clip value
  if (samp>65535) samp=65535;
  if (samp<0) samp=0;
  //send pwm value for sample value
  samp=fakePwm[samp>>11];
  err=(samp&0x7ff); //Save rounding error.
  return i2s_write_sample_nb(samp);
  /*short s=left;
  int x;
  int val=0;
  int w;
  static int i1v=0, i2v=0;
  static int outReg=0;
  for (x=0; x < 32; x++) {
    val<<=1; //next bit
    w=s;
    if (outReg>0) w-=32767; else w+=32767; //Difference 1
    w+=i1v; i1v=w; //Integrator 1
    if (outReg>0) w-=32767; else w+=32767; //Difference 2
    w+=i2v; i2v=w; //Integrator 2
    outReg=w;   //register
    if (w>0) val|=1; //comparator

  }
  return i2s_write_sample_nb(s);*/
}

struct I2S_status_s {
  wavFILE_t wf;
  int16_t buffer[512];
  int bufferlen;
  int buffer_index;
  int playing;
} I2S_WAV;

void wav_stopPlaying()
{
  i2s_end();
  I2S_WAV.playing = false;
  wavClose(&I2S_WAV.wf);
}

bool wav_playing()
{
  return I2S_WAV.playing;
}

void wav_setup()
{
  Serial.println(F("wav_setup"));
  I2S_WAV.bufferlen = -1;
  I2S_WAV.buffer_index = 0;
  I2S_WAV.playing = false;
}

void wav_loop()
{
  bool i2s_full = false;
  int rc;

  while (I2S_WAV.playing && !i2s_full) {
    while (I2S_WAV.buffer_index < I2S_WAV.bufferlen) {
      int16_t pcm = I2S_WAV.buffer[I2S_WAV.buffer_index];
      if (i2s_write_lr_nb(pcm, pcm)) {
        I2S_WAV.buffer_index++;
      }
      else {
        i2s_full = true;
        break;
      }
      if ((I2S_WAV.buffer_index & 0x3F) == 0) yield();
    }
    if (i2s_full) break;

    rc = wavRead(&I2S_WAV.wf, I2S_WAV.buffer, sizeof(I2S_WAV.buffer));
    if (rc > 0) {
      //Serial.printf("wavRead %d\r\n", rc);
      I2S_WAV.bufferlen = rc / sizeof(I2S_WAV.buffer[0]);
      I2S_WAV.buffer_index = 0;
    }
    else {
      Serial.println(F("Stop playing"));
      wav_stopPlaying();
      break;
    }
  }
}

void wav_startPlayingFile(const char *wavfilename)
{
  wavProperties_t wProps;
  int rc;

  //Serial.printf("wav_starPlayingFile(%s)\r\n", wavfilename);
  i2s_begin();
  FastLED.addLeds<WS2812B, 4, GRB>(leds, NUM_LEDS); //Arduino I2S function changes pinmode used by LED
  rc = wavOpen(wavfilename, &I2S_WAV.wf, &wProps);
  //Serial.printf("wavOpen %d\r\n", rc);
  if (rc != 0) {
    Serial.println("wavOpen failed");
    return;
  }
  /*Serial.printf("audioFormat %d\r\n", wProps.audioFormat);
  Serial.printf("numChannels %d\r\n", wProps.numChannels);
  Serial.printf("sampleRate %d\r\n", wProps.sampleRate);
  Serial.printf("byteRate %d\r\n", wProps.byteRate);
  Serial.printf("blockAlign %d\r\n", wProps.blockAlign);
  Serial.printf("bitsPerSample %d\r\n", wProps.bitsPerSample);*/

  i2s_set_rate(wProps.sampleRate);

  I2S_WAV.bufferlen = -1;
  I2S_WAV.buffer_index = 0;
  I2S_WAV.playing = true;
  wav_loop();
}

void showDir(void)
{
  wavFILE_t wFile;
  wavProperties_t wProps;
  int rc;

  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    Serial.println(dir.fileName());
    rc = wavOpen(dir.fileName().c_str(), &wFile, &wProps);
    if (rc == 0) {
      Serial.printf("  audioFormat %d\r\n", wProps.audioFormat);
      Serial.printf("  numChannels %d\r\n", wProps.numChannels);
      Serial.printf("  sampleRate %d\r\n", wProps.sampleRate);
      Serial.printf("  byteRate %d\r\n", wProps.byteRate);
      Serial.printf("  blockAlign %d\r\n", wProps.blockAlign);
      Serial.printf("  bitsPerSample %d\r\n", wProps.bitsPerSample);
      Serial.println();
      wavClose(&wFile);
    }
  }
}

bool loadConfig() {
  File configFile = SPIFFS.open("/config.txt", "r");
  if (!configFile) {
    Serial.println("Failed to open config file\n");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large\n");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println("Failed to parse config file\n");
    return false;
  }
  if(strcmp(json["role"],"gun")==0) {
    hwRole=1;
    Serial.println("Gun Mode\n");
  }
  else {
    hwRole=2;
    Serial.println("Sensor Mode\n");
  }
  return true;
}

void downloadFile(char* url, char* filename){
  char message[80];
  sprintf(message,"Download %s to %s",url,filename);
  Serial.println(message);
  File UploadFile;
  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();
  sprintf(message,"HTTP:%d\n",httpCode);
  Serial.println(message);
  //SPIFFS.format();
  if(httpCode > 0) {
    if(httpCode == HTTP_CODE_OK) {
      Serial.println("Start Download\n");
      // get lenght of document (is -1 when Server sends no Content-Length header)
      int len = http.getSize();
      // create buffer for read
      uint8_t buff[128] = { 0 };
      UploadFile = SPIFFS.open(filename, "w");
      // get tcp stream
      WiFiClient * stream = http.getStreamPtr();
      // read all data from server
      while(http.connected() && (len > 0 || len == -1)) {
        // get available data size
        size_t size = stream->available();
        if(size) {
        // read up to 128 byte
          int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
          // write it to Serial
          UploadFile.write(buff, c);
          if(len > 0) {
            len -= c;
          }
        }
          delay(1);
      }
      Serial.println("Download Finished\n");
      UploadFile.close();
    }
  }
      http.end();
}

void ota() {
  int retries = 10;
  tft.drawString("Maint Mode", 0, 0, 2);
  wl_status_t status = WiFi.begin(OTA_SSID, OTA_PASS);
  while (retries > 0 && status != WL_CONNECTED)
  {
    tft.drawString("Connecting To Wifi ...", 0, 15, 2);
    delay(500);
    tft.fillRect(0,15,120,30,TFT_BLACK);
    delay(500);
    retries--;
    status = WiFi.status();
  }
  tft.drawString("Connected To Wifi", 0, 15, 2);
  char message[20];
  sprintf(message,"Wifi:%d\n",status);
  Serial.println(message);
  tft.drawString("Flashing Firmware", 0, 30, 2);
  ESPhttpUpdate.rebootOnUpdate(false);
  auto reply = ESPhttpUpdate.update(OTA_HOST, 80, OTA_PATH);
  switch (reply)
  {
    case HTTP_UPDATE_FAILED:
      Serial.println("[OTA] Update failed\n");
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("[OTA] Update no updates\n");
      break;
    case HTTP_UPDATE_OK:
      Serial.println("[OTA] Update success.  Rebooting into new firmware...\n");
      tft.drawString("Done, Rebooting", 0, 45, 2);
      SPIFFS.end();
      //ESP.restart();
      break;
  }
  /*if (!SPIFFS.begin()) {
    Serial.println("SPIFFS.begin() failed\n");
    return;
  }*/
  char url[100];
  uint8_t MAC_array[6];
  char MAC_char[18];
  WiFi.macAddress(MAC_array);
  for (int i = 0; i < sizeof(MAC_array); ++i){
      sprintf(MAC_char,"%s%02x",MAC_char,MAC_array[i]);
    }
  char filename[20];
  sprintf(url,"http://192.168.1.207/%s.config",MAC_char);
  sprintf(filename,"/config.txt");
  downloadFile(url,filename);
  sprintf(url,"http://192.168.1.207/test.wav");
  sprintf(filename,"/test.wav");
  downloadFile(url,filename);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200,SERIAL_8N1,SERIAL_TX_ONLY);
  Serial.println();
  FastLED.addLeds<WS2812B, 4, GRB>(leds, NUM_LEDS);
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS.begin() failed\n");
    return;
  }
  if(hwRole==1) {
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
  }
  if(!digitalRead(4))
    ota();
  loadConfig();
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  irsend.begin();

  if(hwRole==2)
    irrecv.enableIRIn();

  encoder.setButtonHeldEnabled(true);
  encoder.setDoubleClickEnabled(true);


  Serial.println(F("\nESP8266 Sound Effects Web Trigger\n"));


  // Confirm track files are present in SPIFFS



  ClickEncoder::Button b = encoder.getButton();
  if (b != ClickEncoder::Open) {
    ota();
    char mystring[10];
    switch (b) {
      case ClickEncoder::Held:
        sprintf(mystring, "Held");
        ota();
        break;
    }
  }
  showDir();
  //wav_setup();
    tft.setTextSize(1);
  //tft.fillScreen(TFT_BLACK);
  tft.fillRect(5,25,45,25,TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);

  tft.drawString("Health", 5, 5, 2);
  //wav_startPlayingFile("/test.wav");
  mesh.setDebugMsgTypes( ERROR | STARTUP );  // set before init() so that you can see startup messages

//mesh.init( MESH_PREFIX, MESH_PASSWORD, MESH_PORT );
mesh.onReceive(&receivedCallback);
mesh.onNewConnection(&newConnectionCallback);
mesh.onChangedConnections(&changedConnectionCallback);
mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
}

void drawMain() {
  tft.setTextSize(1);
  //tft.fillScreen(TFT_BLACK);
  tft.fillRect(0,0,120,45,TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  char mystring[10];
  sprintf(mystring, "Bullets %d", bullets);
  tft.drawString(mystring, 0, 0, 2);
  sprintf(mystring, "Value: %d", value);
  tft.drawString(mystring, 0, 15, 2);

}

void loop() {
  //tft.fillScreen(TFT_BLACK);
  yield();
  wav_loop();
  value += encoder.getValue();

  if (value != last) {
    last = value;
  }

  if (lastService + 1000 < micros()) {
    lastService = micros();
    encoder.service();
    if(bulletdelay)
      bulletdelay--;
    ClickEncoder::Button b = encoder.getButton();
    if (b != ClickEncoder::Open) {
      char mystring[10];
      switch (b) {
        case ClickEncoder::Pressed:
          sprintf(mystring, "Pressed");
          break;
        case ClickEncoder::Released:
          sprintf(mystring, "Released");
          break;
        case ClickEncoder::Clicked:
          sprintf(mystring, "Clicked");
          break;
        case ClickEncoder::Held:
          sprintf(mystring, "Held");
          break;
      }
        tft.fillRect(0,30,100,50,TFT_BLACK);
        tft.drawString(mystring, 0, 30, 2);
    }
  }
  if (lastService2 + 100000 < micros() && hwRole==1) {
        lastService2 = micros();
    drawMain();
  }
 //if(I2S_WAV.playing==false) {
    //wav_startPlayingFile("/test.wav");

  if(digitalRead(5)&&!bulletdelay&&hwRole==1) {
    if(I2S_WAV.playing) {
      wav_stopPlaying();
    }
    bulletdelay=40;
    FastLED.setBrightness(50);
    leds[0] = CRGB::Yellow;

    bullets-=1;
    int before=micros();
    //irsend.sendLG(0x00FFE01FUL, 16);
    int shot=irsend.encodeLTBullet(1,1,1);
    irsend.sendLT(shot,20);
    char mystring[10];
    sprintf(mystring, "IR: %d", micros()-before);
    Serial.println(mystring);
    //tft.drawString(mystring, 0, 30, 2);
    before=micros();
    wav_startPlayingFile("/test.wav");
    sprintf(mystring, "WAV: %d", micros()-before);
    Serial.println(mystring);
    before=micros();
    FastLED.show();
    sprintf(mystring, "LED: %d", micros()-before);
    Serial.println(mystring);
  }
  if(hwRole==1) {
    if(bulletdelay>20) {
      //FastLED.setBrightness((bulletdelay));
      //leds[0] = CRGB(255,bulletdelay,bulletdelay);
      //FastLED.show();
    }
    else {
      FastLED.setBrightness(50);
      leds[0] = CRGB::Black;
      FastLED.show();
      //char mystring[10];
      //sprintf(mystring, "BD: %d", bulletdelay);
      //tft.drawString(mystring, 0, 30, 2);
    }
  }
  if(hwRole==2) {
    if (irrecv.decode(&results)) {
      if (results.decode_type == LT) {
        irrecv.resume();  // Receive the next value
        FastLED.setBrightness(255);
        leds[0] = CRGB::Green;
        FastLED.show();
        delay(10);
        leds[0] = CRGB::Black;
        FastLED.show();
      }
      else {
        FastLED.setBrightness(255);
        leds[0] = CRGB::Red;
        FastLED.show();
        delay(10);
        leds[0] = CRGB::Black;
        FastLED.show();
      }
          }
  }
}
