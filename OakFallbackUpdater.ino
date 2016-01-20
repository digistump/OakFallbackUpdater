SYSTEM_MODE(MANUAL) //we dont want to connect to particle at all in this
#include <Ticker.h>

//#define DEBUG_SETUP

Ticker LEDFlip;

uint8_t LEDState = 0;
void FlipLED(){
   LEDState = !LEDState;
   digitalWrite(0, LEDState);
}


void initVariant() {
    WiFi.disconnect();
    noInterrupts();
    spi_flash_erase_sector(1020);
    spi_flash_erase_sector(1021); 
    interrupts();
}

TODO TRY TO CONNECT TO WIFI, IF NOT REBOOT TO CONFIG



void setup(){
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  Serial.begin(115200);
  pinMode(0,OUTPUT);
  LEDFlip.attach(0.5, FlipLED);
  readBootConfig();

  if(!getDeviceInfo()){
    //switch to rom 0
    ESP.restart();
  }  

  if(first_update_domain[0] == '\0' || first_update_url[0] == '\0' || first_update_fingerprint[0] == '\0'){
    Oak.rebootToConfig();
  }

  #ifdef DEBUG_SETUP
      Serial.println("WIFI");
  #endif

  
  if(!Oak.connect()){//wifiConnect()
    Oak.rebootToConfig();
  }

  if(!Oak.waitForConnected()){
    Oak.rebootToConfig();
  }

  #ifdef DEBUG_SETUP
    Serial.println("GO TO UPDATE");
  #endif
  doFactoryUpdate();//never returns

}


void loop(){

}



void doFactoryUpdate(){

  #ifdef DEBUG_SETUP
    Serial.println("START UPDATE");
  #endif

  LEDFlip.attach(0.1, FlipLED);
  if(doOTAUpdate(first_update_domain,443,first_update_url,first_update_fingerprint,8)){

  TODO ADD CHECK IMAGE FROM OakClass

  #ifdef DEBUG_SETUP
    Serial.println("UPDATE OK - BOOT TO ROM");
  #endif
    
  TODO FIX THIS ALL
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
