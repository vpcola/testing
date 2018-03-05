#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"

#include "lmic.h"
#include "I2CMaster.h"
#include "SSD1306.h"
#include "HTU21D.h"
#include "CayenneLPP.h"

u1_t NWKSKEY[16] = { 0xD6, 0x6D, 0xC7, 0x16, 0xE7, 0x90, 0xB8, 0x39, 0x8A, 0x6A, 0xE9, 0xA4, 0x15, 0x48, 0x21, 0x69 };
u1_t APPSKEY[16] = { 0x29, 0x0E, 0x5C, 0xF7, 0xA1, 0x41, 0x3C, 0x5A, 0x8B, 0xF9, 0x12, 0x96, 0xE3, 0xBA, 0x54, 0x40 };
u4_t DEVADDR = 0x26041938 ;

void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }

static uint8_t mydata[] = "Uela!";

// Using the TTGO ESP32 Lora or Heltec ESP32 Lora board
// https://www.thethingsnetwork.org/forum/t/big-esp32-sx127x-topic-part-2/11973
extern "C" const lmic_pinmap_t lmic_pins = {
    .nss = 18,
    .rst = 14,
    .dio = {26, 33, 32},
    // MISO, MOSI, SCK
    .spi = {19, 27, 5},
};

// I2CMaster (I2C Num, SDA, SCL)
I2CMaster i2c(I2C_NUM_1, GPIO_NUM_4, GPIO_NUM_15);
SSD1306   ssd(i2c, GPIO_NUM_16);
HTU21D    htu(i2c);
CayenneLPP lpp;

const unsigned TX_INTERVAL = 5000;

void do_send(osjob_t * arg)
{
    char tmpbuff[50];
    lpp.reset();

    printf("do_send() called! ... Sending data!\n");
    if (LMIC.opmode & OP_TXRXPEND) {
        printf("OP_TXRXPEND, not sending\n");
    } else {
        float temperature;
        float humidity;
        ssd.Fill(SSD1306::Black);
        // Prepare upstream data transmission at the next possible time.
        if (htu.readTemperature(&temperature))
        {
            sprintf(tmpbuff, "Temperature : %.2f C", temperature);
            ssd.GotoXY(0, 15);
            ssd.Puts(&tmpbuff[0], &Font_7x10, SSD1306::White);
            lpp.addTemperature(1, temperature);
        }
        if (htu.readHumidity(&humidity))
        {
            sprintf(tmpbuff, "Humidity   : %.2f%%", humidity);
            ssd.GotoXY(0,27);
            ssd.Puts(&tmpbuff[0], &Font_7x10, SSD1306::White);
            lpp.addRelativeHumidity(2, humidity);
        }
        ssd.UpdateScreen();

        //LMIC_setTxData2(1, mydata, sizeof(mydata)-1, 0);
        LMIC_setTxData2(1, lpp.getBuffer(), lpp.getSize(), 0); 
        printf("Packet queued\n");
    }
}

void do_receive()
{
    if (LMIC.dataLen > 0)
    {
        // TODO: Copy and process the data from LMIC.dataBeg to a buffer
        printf("Received %d of data\n", LMIC.dataLen);
    }
}

// Callbacks from lmic, needs C linkage
extern "C" void onEvent (ev_t ev) {
    printf("%lld", os_getTime());
    printf(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            printf("EV_SCAN_TIMEOUT\n");
            break;
        case EV_BEACON_FOUND:
            printf("EV_BEACON_FOUND\n");
            break;
        case EV_BEACON_MISSED:
            printf("EV_BEACON_MISSED\n");
            break;
        case EV_BEACON_TRACKED:
            printf("EV_BEACON_TRACKED\n");
            break;
        case EV_JOINING:
            printf("EV_JOINING\n");
            break;
        case EV_JOINED:
            printf("EV_JOINED\n");
            break;
        case EV_RFU1:
            printf("EV_RFU1\n");
            break;
        case EV_JOIN_FAILED:
            printf("EV_JOIN_FAILED\n");
            break;
        case EV_REJOIN_FAILED:
            printf("EV_REJOIN_FAILED\n");
            break;
        case EV_TXCOMPLETE:
            printf("EV_TXCOMPLETE (includes waiting for RX windows)\n");
            if (LMIC.txrxFlags & TXRX_ACK)
              printf("Received ack\n");
            if (LMIC.dataLen) {
              printf("Received %d bytes of payload\n", LMIC.dataLen);
            }
            // Schedule the send job at some dela
            os_setTimedCallback(&LMIC.osjob, os_getTime()+ms2osticks(TX_INTERVAL), FUNC_ADDR(do_send));
            break;
        case EV_LOST_TSYNC:
            printf("EV_LOST_TSYNC\n");
            break;
        case EV_RESET:
            printf("EV_RESET\n");
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            printf("EV_RXCOMPLETE\n");
            do_receive();
            break;
        case EV_LINK_DEAD:
            printf("EV_LINK_DEAD\n");
            break;
        case EV_LINK_ALIVE:
            printf("EV_LINK_ALIVE\n");
            break;
         default:
            // printf("Unknown event: %d\n", ev);
            break;
    }
}

void os_runloop(void * arg) 
{
  do_send(NULL);

  while(1) {
    os_run();
    // vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

extern "C" void app_main(void)
{
  os_init();
  i2c.init();
  ssd.init();
  htu.init();

  LMIC_reset();
  printf("LMIC RESET\n");

  uint8_t appskey[sizeof(APPSKEY)];
  uint8_t nwkskey[sizeof(NWKSKEY)];
  memcpy(appskey, APPSKEY, sizeof(APPSKEY));
  memcpy(nwkskey, NWKSKEY, sizeof(NWKSKEY));
  LMIC_setSession (0x1, DEVADDR, nwkskey, appskey);

#if defined(CFG_eu868)
  LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
  LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI);      // g-band
  LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
  LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
  LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
  LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
  LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
  LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
  LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK, DR_FSK), BAND_MILLI); // g2-band
#elif defined(CFG_us915)
  LMIC_selectSubBand(0); 
#endif

  LMIC_setLinkCheckMode(0);
  LMIC.dn2Dr = DR_SF9;
  LMIC_setDrTxpow(DR_SF7,14);
  
  // Disable channel 1 to 8
  for(int i = 1; i <= 8; i++) LMIC_disableChannel(i);

  xTaskCreate(os_runloop, "os_runloop", 1024 * 2, (void* )0, 10, NULL);
}
