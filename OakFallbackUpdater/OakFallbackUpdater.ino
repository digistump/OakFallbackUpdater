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
#include "WiFiClientSecure.h"
//#define DEBUG_SETUP


const char * update_domain = "oakota.digistump.com";
const char * update_url = "/firmware/firmware_v1.bin";
const char * update_fingerprint = "98 66 d5 5c 3d 4a 49 24 e3 1b 72 8b 8f 2e 65 2e 32 2a 7b 95";

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
//char first_update_domain[65];
//char first_update_url[65];
//char first_update_fingerprint[60];
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
  //memcpy(first_update_domain,deviceConfig->first_update_domain,65);
  //memcpy(first_update_url,deviceConfig->first_update_url,65);
  //memcpy(first_update_fingerprint,deviceConfig->first_update_fingerprint,60);
  memcpy(ssid,deviceConfig->ssid,33);
  memcpy(passcode,deviceConfig->passcode,65);
  channel = deviceConfig->channel;
  return true;
}

uint8 boot_buffer[BOOT_CONFIG_SIZE];
oakboot_config *bootConfig = (oakboot_config*)boot_buffer;

 


Ticker LEDFlip;

uint8_t LEDState = 0;
void FlipLEDFast(){
   LEDState = !LEDState;
   digitalWrite(5, LEDState);
}
uint8_t LED_count = 0;
void FlipLED(){
   LEDState = !LEDState;
   digitalWrite(5, LEDState);
   if(LED_count == 0){
    LEDFlip.detach();
    LEDFlip.attach(0.1,FlipLED);
   }
   else if(LED_count == 5){
    LEDFlip.detach();
    LEDFlip.attach(0.5,FlipLED);
    LED_count = 0;
    return;
   }
   LED_count++;
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
//pin 5 because we are building this with vanilla ESP8266 2.0.0
pinMode(5,OUTPUT);
  LEDFlip.attach(0.5, FlipLED);
  #ifdef DEBUG_SETUP
      Serial.println("START");
      FreeHeap.attach(5, ShowFreeHeap);
    #endif
  readBootConfig();

  //set this before we start so we'll jump back to config on failure
  bootConfig->current_rom = bootConfig->config_rom;
  writeBootConfig();

  if(!getDeviceInfo()){
    //switch to config rom
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
  delay(100);
  uint32_t timeoutTime = millis() + 15000;
  while (WiFi.status() != WL_CONNECTED)
  {
    yield();
    //timeout after 15 seconds and return to main config loop - config on app will fail
    if(millis() > timeoutTime){
        //reboot to config rom as wifi info failed
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


#define NOINLINE __attribute__ ((noinline))

#define ROM_MAGIC    0xe9
#define ROM_MAGIC_NEW1 0xea
#define ROM_MAGIC_NEW2 0x04


// buffer size, must be at least 0x10 (size of rom_header_new structure)
#define BUFFER_SIZE 0x100

// functions we'll call by address
typedef void stage2a(uint32);
typedef void usercode(void);

// standard rom header
typedef struct {
  // general rom header
  uint8 magic;
  uint8 count;
  uint8 flags1;
  uint8 flags2;
  usercode* entry;
} rom_header;

typedef struct {
  uint8* address;
  uint32 length;
} section_header;

// new rom header (irom section first) there is
// another 8 byte header straight afterward the
// standard header
typedef struct {
  // general rom header
  uint8 magic;
  uint8 count; // second magic for new header
  uint8 flags1;
  uint8 flags2;
  uint32 entry;
  // new type rom, lib header
  uint32 add; // zero
  uint32 len; // length of irom section
} rom_header_new;

bool check_image(uint8_t rom_number) {

  uint32 readpos = bootConfig->roms[rom_number];
  
  uint8 buffer[BUFFER_SIZE];
  uint8 sectcount;
  uint8 sectcurrent;
  uint8 *writepos;
  uint8 chksum = CHKSUM_INIT;
  uint32 loop;
  uint32 remaining;
  uint32 romaddr;
  
  rom_header_new *header = (rom_header_new*)buffer;
  section_header *section = (section_header*)buffer;
  
  if (readpos == 0 || readpos == 0xffffffff) {
    //ets_printf("EMPTY");
    return 0;
  }
  
  // read rom header
  //if (SPIRead(readpos, header, sizeof(rom_header_new)) != 0) {
  if (spi_flash_read(readpos, reinterpret_cast<uint32_t*>(header), sizeof(rom_header_new)) != SPI_FLASH_RESULT_OK) {
    //ets_printf("NO_HEADER");
    return 0;
  }
  
  // check header type
  if (header->magic == ROM_MAGIC) {
    // old type, no extra header or irom section to skip over
    romaddr = readpos;
    readpos += sizeof(rom_header);
    sectcount = header->count;
  } else if (header->magic == ROM_MAGIC_NEW1 && header->count == ROM_MAGIC_NEW2) {
    // new type, has extra header and irom section first
    romaddr = readpos + header->len + sizeof(rom_header_new);

    // we will set the real section count later, when we read the header
    sectcount = 0xff;
    // just skip the first part of the header
    // rest is processed for the chksum
    readpos += sizeof(rom_header);
/*
    // skip the extra header and irom section
    readpos = romaddr;
    // read the normal header that follows
    if (SPIRead(readpos, header, sizeof(rom_header)) != 0) {
      //ets_printf("NNH");
      return 0;
    }
    sectcount = header->count;
    readpos += sizeof(rom_header);
*/
  } else {
    //ets_printf("BH");
    return 0;
  }
  
  // test each section
  for (sectcurrent = 0; sectcurrent < sectcount; sectcurrent++) {
    //ets_printf("ST");
    
    // read section header
    if (spi_flash_read(readpos, reinterpret_cast<uint32_t*>(section), sizeof(section_header)) != SPI_FLASH_RESULT_OK) {
      return 0;
    }
    readpos += sizeof(section_header);

    // get section address and length
    writepos = section->address;
    remaining = section->length;
    
    while (remaining > 0) {
      // work out how much to read, up to BUFFER_SIZE
      uint32 readlen = (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE;
      // read the block
      if (spi_flash_read(readpos, reinterpret_cast<uint32_t*>(buffer), readlen) != SPI_FLASH_RESULT_OK) {
        return 0;
      }
      // increment next read and write positions
      readpos += readlen;
      writepos += readlen;
      // decrement remaining count
      remaining -= readlen;
      // add to chksum
      for (loop = 0; loop < readlen; loop++) {
        chksum ^= buffer[loop];
      }
    }
    
//#ifdef BOOT_IROM_CHKSUM
    if (sectcount == 0xff) {
      // just processed the irom section, now
      // read the normal header that follows
      if (spi_flash_read(readpos, reinterpret_cast<uint32_t*>(header), sizeof(rom_header)) != SPI_FLASH_RESULT_OK) {
        //ets_printf("SPI");
        return 0;
      }
      sectcount = header->count + 1;
      readpos += sizeof(rom_header);
    }
//#endif
  }
  
  // round up to next 16 and get checksum
  readpos = readpos | 0x0f;

  if (spi_flash_read(readpos, reinterpret_cast<uint32_t*>(buffer), 1) != SPI_FLASH_RESULT_OK) {
    //ets_printf("CK");
    return 0;

  }
  
  // compare calculated and stored checksums
  if (buffer[0] != chksum) {
    //ets_printf("CKF");
    return 0;
  }
  
  return 1;
}

void doFactoryUpdate(){

      #ifdef DEBUG_SETUP
      Serial.println("START UPDATE");
    #endif

  LEDFlip.detach();
  LEDFlip.attach(0.1, FlipLEDFast);
  uint8_t flashSlot = getOTAFlashSlot();
  if(doOTAUpdate(update_domain,443,update_url,update_fingerprint,flashSlot)){


    //verify image here?
    if(check_image(flashSlot) == true && check_image(flashSlot+2) == true)
    {


      #ifdef DEBUG_SETUP
        Serial.println("UPDATE OK - BOOT TO ROM");
      #endif


      bootConfig->current_rom = bootConfig->program_rom;
      bootConfig->config_rom = flashSlot; 
      bootConfig->update_rom = flashSlot+2; 
      writeBootConfig();
    }
    else{

      #ifdef DEBUG_SETUP
        Serial.println("CORRUPT IMAGE");
      #endif

    }
      
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
  
  uint8_t max_connect_tries = 5;
  while(!ota_client.connect(host,port) && max_connect_tries>0){
    max_connect_tries--;
  }
  if(max_connect_tries <= 0){
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

uint8_t getOTAFlashSlot(){
    if(bootConfig->program_rom != 0 && bootConfig->config_rom != 0)
      return 0;
    else if(bootConfig->program_rom != 4 && bootConfig->config_rom != 4)
      return 4;
    else
      return 8;
}
