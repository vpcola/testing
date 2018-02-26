#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"

#include "lmic.h"

u1_t NWKSKEY[16] = { 0xD6, 0x6D, 0xC7, 0x16, 0xE7, 0x90, 0xB8, 0x39, 0x8A, 0x6A, 0xE9, 0xA4, 0x15, 0x48, 0x21, 0x69 };
u1_t APPSKEY[16] = { 0x29, 0x0E, 0x5C, 0xF7, 0xA1, 0x41, 0x3C, 0x5A, 0x8B, 0xF9, 0x12, 0x96, 0xE3, 0xBA, 0x54, 0x40 };
u4_t DEVADDR = 0x26041938 ;

void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }

static uint8_t mydata[] = "Uela!";

// Using the TTGO ESP32 Lora or Heltec ESP32 Lora board
// https://www.thethingsnetwork.org/forum/t/big-esp32-sx127x-topic-part-2/11973
const lmic_pinmap_t lmic_pins = {
    .nss = 18,
    .rst = 14,
    .dio = {26, 33, 22},
    // MISO, MOSI, SCK
    .spi = {19, 27, 5},
};

const unsigned TX_INTERVAL = 30;

void onEvent (ev_t ev) {
    printf("%d", os_getTime());
    printf(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            printf("EV_SCAN_TIMEOUT");
            break;
        case EV_BEACON_FOUND:
            printf("EV_BEACON_FOUND");
            break;
        case EV_BEACON_MISSED:
            printf("EV_BEACON_MISSED");
            break;
        case EV_BEACON_TRACKED:
            printf("EV_BEACON_TRACKED");
            break;
        case EV_JOINING:
            printf("EV_JOINING");
            break;
        case EV_JOINED:
            printf("EV_JOINED");
            break;
        case EV_RFU1:
            printf("EV_RFU1");
            break;
        case EV_JOIN_FAILED:
            printf("EV_JOIN_FAILED");
            break;
        case EV_REJOIN_FAILED:
            printf("EV_REJOIN_FAILED");
            break;
        case EV_TXCOMPLETE:
            printf("EV_TXCOMPLETE (includes waiting for RX windows)");
            if (LMIC.txrxFlags & TXRX_ACK)
              printf("Received ack");
            if (LMIC.dataLen) {
              printf("Received %d bytes fo payload", LMIC.dataLen);
            }

            if (LMIC.opmode & OP_TXRXPEND) {
                printf("OP_TXRXPEND, not sending");
            } else {
                // Prepare upstream data transmission at the next possible time.
                LMIC_setTxData2(1, mydata, sizeof(mydata)-1, 0);
                printf("Packet queued");
            }
            break;
        case EV_LOST_TSYNC:
            printf("EV_LOST_TSYNC");
            break;
        case EV_RESET:
            printf("EV_RESET");
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            printf("EV_RXCOMPLETE");
            break;
        case EV_LINK_DEAD:
            printf("EV_LINK_DEAD");
            break;
        case EV_LINK_ALIVE:
            printf("EV_LINK_ALIVE");
            break;
         default:
            printf("Unknown event: %d", ev);
            break;
    }
}

esp_err_t event_handler(void *ctx, system_event_t *event)
{
    return ESP_OK;
}

void os_runloop(void) {

  if (LMIC.opmode & OP_TXRXPEND) {
      printf("OP_TXRXPEND, not sending");
  } else {
      // Prepare upstream data transmission at the next possible time.
      LMIC_setTxData2(1, mydata, sizeof(mydata)-1, 0);
      printf("Packet queued");
  }


  while(1) {
    os_run();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void app_main(void)
{
  os_init();

  LMIC_reset();
  printf("LMIC RESET");

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
  // Use the first channel - 0, us does not have SF12
  LMIC_setupChannel(0, 902300000, DR_RANGE_MAP(DR_SF10, DR_SF8C), 0); // band unused
#endif

  LMIC_setLinkCheckMode(0);
  LMIC.dn2Dr = DR_SF9;
  LMIC_setDrTxpow(DR_SF7,14);
  
  // Disable channel 1 to 8
  for(int i = 1; i <= 8; i++) LMIC_disableChannel(i);

  xTaskCreate(os_runloop, "os_runloop", 1024 * 2, (void* )0, 10, NULL);
}
