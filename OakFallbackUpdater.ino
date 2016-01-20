#ifdef __cplusplus
extern "C" {
#endif
  #include <c_types.h>
  #include <user_interface.h>
  #include <mem.h>
  #include <osapi.h>
  #include "oakboot.h"
#ifdef __cplusplus
}
#endif


#include <Ticker.h>
#include "ESP8266WiFi.h"

//#define DEBUG_SETUP





#ifndef MAX_ROM_SIZE //this becomes variable in the new scheme
  #define MAX_ROM_SIZE (0x100000-0x2000)
#endif

#define SECTOR_SIZE 0x1000
#define DEVICE_CONFIG_SECTOR 256
#define DEVICE_BACKUP_CONFIG_SECTOR 512
#define DEVICE_CHKSUM_INIT 0xee
#define DEVICE_MAGIC 0xf0

#define BOOT_CONFIG_SIZE 92
#define DEVICE_CONFIG_SIZE 3398

typedef struct {
  //can cut off here if needed
  char device_id[25];     //device id in hex
  char claim_code[65];   // server public key
  uint8 claimed;   // server public key
  uint8 device_private_key[1216];  // device private key
  uint8 device_public_key[384];   // device public key
  uint8 server_public_key[768]; //also contains the server address at offset 384
  uint8 server_address_type;   //domain or ip of cloud server
  uint8 server_address_length;   //domain or ip of cloud server
  char server_address_domain[254];   //domain or ip of cloud server
  uint8 padding;
  uint32 server_address_ip;   //[4]//domain or ip of cloud server
  unsigned short firmware_version;  
  unsigned short system_version;     //
  char version_string[33];    //
  uint8 reserved_flags[32];    //
  uint8 reserved1[32];
  uint8 product_store[24];    
  char ssid[33]; //ssid and terminator
  char passcode[65]; //passcode and terminator
  uint8 channel; //channel number
  int32 third_party_id;    //
  char third_party_data[256];     //
  char first_update_domain[65];
  char first_update_url[65];
  char first_update_fingerprint[60];
  uint8 current_rom_scheme[1];
  uint8 padding2[1];
  uint8 magic;
  uint8 chksum; 
  //uint8 reserved2[698]; 
} oak_config; 

char device_id[25];
char first_update_domain[65];
char first_update_url[65];
char first_update_fingerprint[60];
  char ssid[33]; //ssid and terminator
  char passcode[65]; //passcode and terminator
  uint8 channel; 

bool getDeviceInfo(){
  uint8 config_buffer[DEVICE_CONFIG_SIZE];
  oak_config *deviceConfig = (oak_config*)config_buffer;
  noInterrupts();


  spi_flash_read(DEVICE_CONFIG_SECTOR * SECTOR_SIZE, reinterpret_cast<uint32_t*>(config_buffer), DEVICE_CONFIG_SIZE);

  if(deviceConfig->magic != DEVICE_MAGIC || deviceConfig->chksum != calc_device_chksum((uint8*)deviceConfig, (uint8*)&deviceConfig->chksum)){
    //load the backup and copy to main
    spi_flash_read(DEVICE_BACKUP_CONFIG_SECTOR * SECTOR_SIZE, reinterpret_cast<uint32_t*>(config_buffer), DEVICE_CONFIG_SIZE);
    spi_flash_erase_sector(DEVICE_CONFIG_SECTOR);
    spi_flash_write(DEVICE_CONFIG_SECTOR * SECTOR_SIZE, reinterpret_cast<uint32_t*>(config_buffer), DEVICE_CONFIG_SIZE);
  }
  interrupts();

  if(deviceConfig->magic != DEVICE_MAGIC || deviceConfig->chksum != calc_device_chksum((uint8*)deviceConfig, (uint8*)&deviceConfig->chksum)){
    return false;
  }


  memcpy(device_id,deviceConfig->device_id,25);
  memcpy(first_update_domain,deviceConfig->first_update_domain,65);
  memcpy(first_update_url,deviceConfig->first_update_url,65);
  memcpy(first_update_fingerprint,deviceConfig->first_update_fingerprint,60);
  memcpy(ssid,deviceConfig->ssid,33);
  memcpy(passcode,deviceConfig->passcode,65);
  channel = deviceConfig->channel;
  return true;
}

uint8 boot_buffer[BOOT_CONFIG_SIZE];
rboot_config *bootConfig = (rboot_config*)boot_buffer;

 


Ticker LEDFlip;

uint8_t LEDState = 0;
void FlipLED(){
   LEDState = !LEDState;
   digitalWrite(5, LEDState);
}


void initVariant() {
    WiFi.disconnect();
    noInterrupts();
    spi_flash_erase_sector(1020);
    spi_flash_erase_sector(1021); 
    interrupts();
}

  #ifdef DEBUG_SETUP
Ticker FreeHeap;
void ShowFreeHeap(){
      Serial.println(ESP.getFreeHeap());
      }
    #endif




void setup(){
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
Serial.begin(115200);
pinMode(5,OUTPUT);
  LEDFlip.attach(0.5, FlipLED);
  #ifdef DEBUG_SETUP
      Serial.println("START");
      FreeHeap.attach(5, ShowFreeHeap);
    #endif
  readBootConfig();

  //set these before we start so we'll jump back to config on failure
  bootConfig->current_rom = 0;
  bootConfig->config_rom = 0; 
  bootConfig->update_rom = 0;
  bootConfig->program_rom = 0;

  writeBootConfig();

  if(!getDeviceInfo()){
    //switch to rom 0
    ESP.restart();
  }  

  if(first_update_domain[0] == '\0' || first_update_url[0] == '\0' || first_update_fingerprint[0] == '\0'){
    ESP.restart();
  }

  #ifdef DEBUG_SETUP
      Serial.println("WIFI");
    #endif

  
  if(passcode[0] != '\0' && channel > 0){
    WiFi.begin(ssid,passcode, channel);
  }
  else if(passcode[0] != '\0'){
    WiFi.begin(ssid,passcode);
  }
  else if(channel > 0){
    WiFi.begin(ssid, NULL, channel);
  }
  else if(ssid[0] != '\0'){
    WiFi.begin(ssid);
  }
  else{
    ESP.restart();
  }

  #ifdef DEBUG_SETUP
      Serial.println("WIFI CONNECT");
    #endif
  //wait for connect
  uint32_t timeoutTime = millis() + 15000;
  while (WiFi.status() != WL_CONNECTED)
  {
    yield();
    //timeout after 15 seconds and return to main config loop - config on app will fail
    if(millis() > timeoutTime){
        ESP.restart();
      }
      delay(100);
  }
  #ifdef DEBUG_SETUP
      Serial.println("GO TO UPDATE");
    #endif
  doFactoryUpdate();//never returns

}


void loop(){

}

static uint8 calc_device_chksum(uint8 *start, uint8 *end) {
  uint8 chksum = DEVICE_CHKSUM_INIT;
  while(start < end) {
    chksum ^= *start;
    start++;
  }
  return chksum;
}


static uint8 calc_chksum(uint8 *start, uint8 *end) {
  uint8 chksum = CHKSUM_INIT;
  while(start < end) {
    chksum ^= *start;
    start++;
  }
  return chksum;
}

void doFactoryUpdate(){

      #ifdef DEBUG_SETUP
      Serial.println("START UPDATE");
    #endif

  LEDFlip.attach(0.1, FlipLED);
  if(doOTAUpdate(first_update_domain,443,first_update_url,first_update_fingerprint,8)){

    #ifdef DEBUG_SETUP
      Serial.println("UPDATE OK - BOOT TO ROM");
    #endif
    

    bootConfig->current_rom = 8;
    bootConfig->config_rom = 0; //IMPORTANT TODO ON FINAL we're going to set this as factory - so that we come back here if the other one fails, but the first thing the other should do is set itself to factory and set user_rom to 1
    bootConfig->update_rom = 0; //IMPORTANT TODO ON FINAL we're going to set this as factory - so that we come back here if the other one fails, but the first thing the other should do is set itself to factory and set user_rom to 1
    bootConfig->program_rom = 8;
      //firmware_version = 1; do this on first boot of new firmware
    writeBootConfig();
      
  }
  else{
    #ifdef DEBUG_SETUP
      Serial.println("UPDATE FAILED");
    #endif
  }
  LEDFlip.detach();
  delay(100);
  ESP.restart();

}

bool doOTAUpdate(const char * host, uint16_t port, const char * url, const char * fingerprint, uint8_t upgrade_slot){

  #ifdef DEBUG_SETUP
    Serial.println("START");
  #endif

  //make sure isn't current rom, make sure rom address is at segment divison
  /*void *f = malloc(SECTOR_SIZE);
  if(!f){
    #ifdef DEBUG_SETUP
      Serial.println("OUT OF MEMORY");
    #endif
    return false;
  }*/
  //free(f);
  
  
  uint32_t length_remaining = 0;
  uint16_t current_sector = bootConfig->roms[upgrade_slot]/SECTOR_SIZE;
  uint16_t max_sector = (bootConfig->roms[upgrade_slot]+MAX_ROM_SIZE)/SECTOR_SIZE;

  #ifdef DEBUG_SETUP
  Serial.println(ESP.getFreeHeap());
      Serial.println("current_sector");
      Serial.println(current_sector);
      Serial.println("max_sector");
      Serial.println(max_sector);
      Serial.println("host");
      Serial.println(host);
      Serial.println("port");
      Serial.println(port);
    #endif
#ifdef DEBUG_SETUP
  Serial.println("do we have enough?");
  #endif
  //free(freeholder);
  WiFiClientSecure ota_client;
#ifdef DEBUG_SETUP
  Serial.println("ota_client");
  Serial.println(ESP.getFreeHeap());
  FreeHeap.detach();
#endif
    // configure time
  configTime(3 * 3600, 0, "pool.ntp.org");
  
  if(!ota_client.connect(host,port)){
    #ifdef DEBUG_SETUP
      Serial.println("COULD NOT CONNECT");
      Serial.println(ESP.getFreeHeap());
    #endif
      
      return false;
    }
    #ifdef DEBUG_SETUP
  Serial.println("verify");
  #endif


    if (!ota_client.verify(fingerprint, host)) {

    #ifdef DEBUG_SETUP
      Serial.println("COULD NOT VERIFY THUMBPRINT");
      Serial.println(ESP.getFreeHeap());
    #endif
      
      return false;
    }

  //allocate memory for buffer
  uint8_t *data = new uint8_t[SECTOR_SIZE];

  //make request
  ota_client.print(String("POST ") + url + " HTTP/1.1\r\n" +
             "Host: " + host + "\r\n" + 
             "Content-Length: 27\r\n" + 
             "Content-Type: application/x-www-form-urlencoded\r\n" + 
             "Connection: close\r\n\r\n" +
             "id=" + device_id);



  delay(100);

  ota_client.setTimeout(10000);




  #ifdef DEBUG_SETUP
    Serial.println("header");
  #endif
  String response = ota_client.readStringUntil('\n');
  int OK_code = response.indexOf("HTTP/1.1 200");
  int NM_code = response.indexOf("HTTP/1.1 304");
  if(NM_code<0 && OK_code<0)
    return true;
  response = "";
  int content_length = 0;
  bool gotContentLength = false;


  //TOFU ADD TIMEOUT HERE
  uint32_t headerTimeout = millis() + 15000;
  while(1){

    response = ota_client.readStringUntil('\n');
    #ifdef DEBUG_SETUP
      Serial.println(response);
    #endif
    if(gotContentLength == false){
      content_length = response.indexOf("Content-Length: ");
      #ifdef DEBUG_SETUP
        Serial.println(content_length);
      #endif
      if(content_length >= 0){
        #ifdef DEBUG_SETUP
          Serial.println("CL!:");
          Serial.println(response.substring(content_length+16));
        #endif
        length_remaining = response.substring(content_length+16).toInt();
        //update_content_len = 203824;
        gotContentLength = true;

      }
    }
    else if(response.length() == 1){
      //end of header
      break;
    }
    yield();
    if(millis()>headerTimeout){
      #ifdef DEBUG_SETUP
        Serial.println("header timeout");
      #endif
      return false;
    }
  }


  //we are now at the start of the data
  uint16_t dataIndex;
  uint16_t extra_bytes;

  #ifdef DEBUG_SETUP
    Serial.println("start write");
  #endif
  //while(1){yield();}
  while(length_remaining > 0){
    #ifdef DEBUG_SETUP
      Serial.println("start sector\r\nremaining");
      Serial.println(length_remaining);
    #endif
    yield();
    dataIndex = 0;
    extra_bytes = 0;
    //data = {0};


    //memset(&data[0], 0, sizeof(data));
    
    if(length_remaining<SECTOR_SIZE){
      extra_bytes = SECTOR_SIZE-length_remaining;
      #ifdef DEBUG_SETUP
        Serial.println("extra bytes");
        Serial.println(extra_bytes);
      #endif
    }


    while(dataIndex + extra_bytes < SECTOR_SIZE){
      
      if(!ota_client.available()){
        uint32_t waitTimeout = millis() + 10000;
        while(!ota_client.available()){
          yield();
          if(millis()>waitTimeout){
            #ifdef DEBUG_SETUP
              Serial.println("read timeout");
            #endif
            return false;
          }
        }
      }
      data[dataIndex] = ota_client.read();
      dataIndex++;

    }

    length_remaining -= dataIndex;

    //if extra bytes add them
    while(extra_bytes>0){
      data[dataIndex] = '\0';
      dataIndex++;
      extra_bytes--;
    }

    

    //erase and write sector
    #ifdef DEBUG_SETUP
      Serial.println("erase and write");
    #endif

    noInterrupts();
      if(spi_flash_erase_sector(current_sector) != SPI_FLASH_RESULT_OK){
        #ifdef DEBUG_SETUP
          Serial.println("erase failed");
          Serial.println(current_sector);
        #endif
          return false;
      }

      spi_flash_write(current_sector * SECTOR_SIZE, reinterpret_cast<uint32_t*>(data), SECTOR_SIZE);
      //SPIWrite(current_sector * SECTOR_SIZE, data, SECTOR_SIZE);
      interrupts();
      current_sector++;
    #ifdef DEBUG_SETUP
      Serial.println("sector done");
    #endif
             

    
  }

  if(current_sector != max_sector){
    #ifdef DEBUG_SETUP
      Serial.println("clear last sector");
    #endif
    noInterrupts();
      spi_flash_erase_sector(current_sector);
      interrupts();
  }

  ota_client.flush();
  ota_client.stop();
  #ifdef DEBUG_SETUP
    Serial.println("yay");
  #endif

  return true;



}

bool readBootConfig(){
noInterrupts();
  spi_flash_read(BOOT_CONFIG_SECTOR * SECTOR_SIZE, reinterpret_cast<uint32_t*>(boot_buffer), BOOT_CONFIG_SIZE);

  if(bootConfig->magic != BOOT_CONFIG_MAGIC || bootConfig->chksum != calc_chksum((uint8*)bootConfig, (uint8*)&bootConfig->chksum)){

    //load the backup and copy to main
    spi_flash_read(BOOT_BACKUP_CONFIG_SECTOR * SECTOR_SIZE, reinterpret_cast<uint32_t*>(boot_buffer), BOOT_CONFIG_SIZE);
    spi_flash_erase_sector(BOOT_CONFIG_SECTOR);
    spi_flash_write(BOOT_CONFIG_SECTOR * SECTOR_SIZE, reinterpret_cast<uint32_t*>(boot_buffer), BOOT_CONFIG_SIZE);

  }
  interrupts();

  if(bootConfig->magic != BOOT_CONFIG_MAGIC || bootConfig->chksum != calc_chksum((uint8*)bootConfig, (uint8*)&bootConfig->chksum)){
    return false;
  }

  return true;
}




void writeBootConfig(){
  noInterrupts();
    bootConfig->chksum = calc_chksum((uint8*)bootConfig,(uint8*)&bootConfig->chksum);
    spi_flash_erase_sector(BOOT_CONFIG_SECTOR);
    spi_flash_write(BOOT_CONFIG_SECTOR * SECTOR_SIZE, reinterpret_cast<uint32_t*>(boot_buffer), BOOT_CONFIG_SIZE);
    spi_flash_erase_sector(BOOT_BACKUP_CONFIG_SECTOR);
    spi_flash_write(BOOT_BACKUP_CONFIG_SECTOR * SECTOR_SIZE, reinterpret_cast<uint32_t*>(boot_buffer), BOOT_CONFIG_SIZE);
    interrupts();
    
  }

