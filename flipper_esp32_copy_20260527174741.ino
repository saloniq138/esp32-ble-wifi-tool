#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <Adafruit_PN532.h>
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"

// --- INICJALIZACJA EKRANÓW ---
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

#define TFT_CS     5
#define TFT_RST    16
#define TFT_DC     17
#define ST77XX_DARKGREY 0x39E7
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// --- PINY NFC ---
#define SOFT_I2C_SDA 32
#define SOFT_I2C_SCL 33
TwoWire NFC_Wire = TwoWire(1);
Adafruit_PN532 nfc(SOFT_I2C_SDA, SOFT_I2C_SCL, &NFC_Wire);

// --- PINY STEROWANIA ---
const int POT_PIN   = 34;
const int TOUCH_PIN = 13;

// --- MENU GŁÓWNE ---
const int ILOSC_OPCJI = 7;
const char* menuItems[ILOSC_OPCJI] = {
  "1. Skaner Wi-Fi",
  "2. GENERUJ 20+ SIECI",
  "3. RADAR / Fox-Hunt",
  "4. Klonuj / Emuluj NFC",
  "5. Bruce: Skaner BLE",
  "6. Bruce: Wardriving",
  "7. BLE Traffic Monitor"
};

int  wybranaOpcja  = 0;
int  ostatniaOpcja = -1;
bool wTrakciePracy = false;
bool apUruchomiony = false;
bool nfcDostepne   = false;

float obecnaPozycjaY   = 35.0;
float docelowaPozycjaY = 35.0;

// --- BLE ---
volatile int bleDevicesCount = 0;
int pakietyStandardowe = 0;
int pakietyIBeacon     = 0;

struct BleDevice {
  char mac[18];
  int  rssi;
  char name[15];
};
BleDevice znalezioneUrzadzenia[3];

// --- SIECI FAŁSZYWE ---
const int  ILOSC_SIECI = 20;
const char* zabawneSieci[ILOSC_SIECI] = {
  "Dora the Explorer of Internet", "Area 51 - Sektor Badawczy",
  "Bunkier przeciwatomowy nr 4",   "Podziemna silownia sasiada",
  "Darmowe Wi-Fi dla wybranych",   "Rzadowy Emiter Monitorujacy",
  "Zdalne sterowanie golebiami",   "CBA - Mobilna Stacja Operacyjna",
  "Brak internetu - idz na spacer","Virus.EXE - Kliknij aby pobrac",
  "Tajne laboratorium Babci",      "Satelita Szpiegowski NASA-3",
  "Proba mikrofonu 1 2 3...",      "Ladowanie akumulatora ptakow",
  "Wejscie do Matrixa",            "WIFI_SASIADA_JEST_LEPSZE",
  "Instytut Paranormalny",         "Twoj telefon Cie podsluchuje",
  "Strefa wolna od 5G",            "Nie dotykaj tej sieci"
};

int                aktualnyIndeksNazwy = 0;
unsigned long      ostatniaZmianaNazwy = 0;
const unsigned long INTERWAL_ROTACJI = 25;
String             namierzanaSiec = "";

// -------------------------------------------------------
// CALLBACK BLE
// -------------------------------------------------------
void ble_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  if (event != ESP_GAP_BLE_SCAN_RESULT_EVT) return;
  if (param->scan_rst.search_evt != ESP_GAP_SEARCH_INQ_RES_EVT) return;

  uint8_t *adv_data     = param->scan_rst.ble_adv;
  uint8_t  adv_data_len = param->scan_rst.adv_data_len;
  bool     maDaneProducenta = false;

  for (int idx = 0; idx < adv_data_len; ) {
    uint8_t length = adv_data[idx];
    if (length == 0 || idx + length >= adv_data_len) break;
    if (adv_data[idx + 1] == 0xFF) { maDaneProducenta = true; break; }
    idx += length + 1;
  }

  if (maDaneProducenta) pakietyIBeacon++;
  else                  pakietyStandardowe++;

  if (bleDevicesCount < 3) {
    snprintf(znalezioneUrzadzenia[bleDevicesCount].mac, 18,
             "%02X:%02X:%02X:%02X:%02X:%02X",
             param->scan_rst.bda[0], param->scan_rst.bda[1],
             param->scan_rst.bda[2], param->scan_rst.bda[3],
             param->scan_rst.bda[4], param->scan_rst.bda[5]);
    znalezioneUrzadzenia[bleDevicesCount].rssi = param->scan_rst.rssi;

    uint8_t  len_tmp  = 0;
    uint8_t *name_ptr = esp_ble_resolve_adv_data(param->scan_rst.ble_adv,
                                                 ESP_BLE_AD_TYPE_NAME_CMPL,
                                                 &len_tmp);
    if (name_ptr && len_tmp > 0) {
      int l = min((int)len_tmp, 14);
      memcpy(znalezioneUrzadzenia[bleDevicesCount].name, name_ptr, l);
      znalezioneUrzadzenia[bleDevicesCount].name[l] = '\0';
    } else {
      strncpy(znalezioneUrzadzenia[bleDevicesCount].name, "Nieznane", 15);
    }
    bleDevicesCount++;
  }
}

// -------------------------------------------------------
// HELPERS
// -------------------------------------------------------
void czekajNaPuszczenieCaly() {
  delay(50);
  while (digitalRead(TOUCH_PIN) == HIGH) delay(10);
  delay(50);
}

void czekajNaWcisniecie() {
  while (digitalRead(TOUCH_PIN) == LOW) delay(10);
  czekajNaPuszczenieCaly();
}

void powrotDoMenu() {
  wTrakciePracy = false;
  ostatniaOpcja = -1;
  tft.fillScreen(ST77XX_BLACK);
}

String konwertujZabezpieczenie(wifi_auth_mode_t typ) {
  switch (typ) {
    case WIFI_AUTH_OPEN:            return "OPEN";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA2";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/3";
    default:                        return "SEC";
  }
}

// -------------------------------------------------------
// SETUP
// -------------------------------------------------------
void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT);

  // ZABEZPIECZENIE: Zmuszamy Wi-Fi do pracy w RAM, aby szybka rotacja nie zniszczyła pamięci Flash
  esp_wifi_set_storage(WIFI_STORAGE_RAM);

  // NVS wymagane przez BT
  nvs_flash_init();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  oled.begin();

  tft.initR(INITR_MINI160x80);
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);

  NFC_Wire.begin(SOFT_I2C_SDA, SOFT_I2C_SCL, 100000);
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (versiondata) {
    nfc.SAMConfig();
    nfcDostepne = true;
  }
}

// -------------------------------------------------------
// OPCJA 1: SKANER WI-FI
// -------------------------------------------------------
void skanujWiFi() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(10, 10);
  tft.setTextColor(ST77XX_GREEN);
  tft.print("Skanowanie sieci...");

  int n = WiFi.scanNetworks();
  tft.fillScreen(ST77XX_BLACK);
  tft.drawRect(0, 0, 160, 80, ST77XX_GREEN);

  if (n <= 0) {
    tft.setCursor(10, 35);
    tft.print("Brak sieci!");
  } else {
    int wyswietl = min(n, 4);
    for (int i = 0; i < wyswietl; i++) {
      tft.setCursor(5, 8 + i * 16);
      tft.setTextColor(ST77XX_WHITE);
      String ssid = WiFi.SSID(i);
      if (ssid.length() > 12) ssid = ssid.substring(0, 10) + "..";
      tft.printf("%d.%s (%d)", i + 1, ssid.c_str(), WiFi.RSSI(i));
    }
  }

  tft.setCursor(5, 72);
  tft.setTextColor(ST77XX_DARKGREY);
  tft.print("Dotknij, aby wyjsc");

  czekajNaWcisniecie();
  powrotDoMenu();
}

// -------------------------------------------------------
// OPCJA 2: GENERATOR SIECI (AP rotacja)
// -------------------------------------------------------
void superszybkaZmienNazwe(const char* nowaNazwa) {
  wifi_config_t config;
  if (esp_wifi_get_config(WIFI_IF_AP, &config) == ESP_OK) {
    memset(config.ap.ssid, 0, sizeof(config.ap.ssid));
    strncpy((char*)config.ap.ssid, nowaNazwa, sizeof(config.ap.ssid) - 1);
    config.ap.ssid_len = strlen(nowaNazwa);
    esp_wifi_set_config(WIFI_IF_AP, &config);
  }
}

void obslugaSzybkiejRotacji() {
  if (!apUruchomiony) return;
  unsigned long teraz = millis();
  if (teraz - ostatniaZmianaNazwy < INTERWAL_ROTACJI) return;

  ostatniaZmianaNazwy = teraz;
  aktualnyIndeksNazwy = (aktualnyIndeksNazwy + 1) % ILOSC_SIECI;
  superszybkaZmienNazwe(zabawneSieci[aktualnyIndeksNazwy]);

  // Odśwież ekran tylko przy pełnym obrocie
  if (aktualnyIndeksNazwy == 0) {
    tft.fillScreen(ST77XX_BLACK);
    tft.drawRect(0, 0, 160, 80, ST77XX_BLUE);
    tft.setTextColor(ST77XX_GREEN);
    tft.setCursor(10, 20);
    tft.print("GENERATOR AKTYWNY");
    tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(10, 40);
    tft.printf("Rozglasza sieci: %d", ILOSC_SIECI);
    tft.setTextColor(ST77XX_DARKGREY);
    tft.setCursor(10, 65);
    tft.print("Dotknij, aby wylaczyc");
  }
}

// -------------------------------------------------------
// OPCJA 3: RADAR / FOX-HUNT
// -------------------------------------------------------
void uruchomRadar() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(10, 35);
  tft.setTextColor(ST77XX_ORANGE);
  tft.print("Szukanie 'Lisa'...");

  int n = WiFi.scanNetworks();
  if (n <= 0) {
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(10, 35);
    tft.print("Brak celow w poblizu!");
    delay(2000);
    powrotDoMenu();
    return;
  }

  int wybranyCel = 0;
  int staryCel   = -1;

  // Wybór celu pokrętłem
  czekajNaPuszczenieCaly();
  while (digitalRead(TOUCH_PIN) == LOW) {
    int pot = analogRead(POT_PIN);
    wybranyCel = map(pot, 0, 4095, 0, n - 1);

    if (wybranyCel != staryCel) {
      tft.fillScreen(ST77XX_BLACK);
      tft.drawRect(0, 0, 160, 80, ST77XX_ORANGE);
      tft.setCursor(10, 15);
      tft.setTextColor(ST77XX_YELLOW);
      tft.print("Wybierz cel:");
      tft.setCursor(10, 45);
      tft.setTextColor(ST77XX_WHITE);
      String ssid = WiFi.SSID(wybranyCel);
      if (ssid.length() > 16) ssid = ssid.substring(0, 14) + "..";
      tft.print(ssid);
      staryCel = wybranyCel;
    }
    delay(30);
  }

  namierzanaSiec = WiFi.SSID(wybranyCel);
  czekajNaPuszczenieCaly();

  // Śledzenie sygnału
  while (digitalRead(TOUCH_PIN) == LOW) {
    int liczbaSieci = WiFi.scanNetworks(false, false);
    int aktualneRSSI = -100;

    for (int i = 0; i < liczbaSieci; i++) {
      if (WiFi.SSID(i) == namierzanaSiec) {
        aktualneRSSI = WiFi.RSSI(i);
        break;
      }
    }

    int szerokoscPaska = map(constrain(aktualneRSSI, -100, -30), -100, -30, 0, 140);
    uint16_t kolorRadaru = ST77XX_RED;
    if (aktualneRSSI > -75) kolorRadaru = ST77XX_YELLOW;
    if (aktualneRSSI > -55) kolorRadaru = ST77XX_GREEN;

    tft.fillScreen(ST77XX_BLACK);
    tft.drawRect(0, 0, 160, 80, kolorRadaru);

    tft.setCursor(10, 15);
    tft.setTextColor(ST77XX_WHITE);
    String skr = namierzanaSiec;
    if (skr.length() > 14) skr = skr.substring(0, 12) + "..";
    tft.printf("CEL: %s", skr.c_str());

    tft.drawRect(10, 25, 140, 15, ST77XX_WHITE);
    if (szerokoscPaska > 0)
      tft.fillRect(10, 25, szerokoscPaska, 15, kolorRadaru);

    tft.setCursor(10, 58);
    if (aktualneRSSI == -100) {
      tft.setTextColor(ST77XX_RED);
      tft.print("Sygnal: ZGUBIONO!");
    } else {
      tft.setTextColor(kolorRadaru);
      tft.printf("Sygnal: %d dBm", aktualneRSSI);
    }

    tft.setCursor(10, 70);
    tft.setTextColor(ST77XX_DARKGREY);
    tft.print("Dotknij, aby wyjsc");

    for (int d = 0; d < 15; d++) {
      if (digitalRead(TOUCH_PIN) == HIGH) break;
      delay(20);
    }
  }

  czekajNaPuszczenieCaly();
  powrotDoMenu();
}

// -------------------------------------------------------
// OPCJA 4: KLONER / EMULATOR NFC
// -------------------------------------------------------
void uruchomKlonerNFC() {
  tft.fillScreen(ST77XX_BLACK);
  tft.drawRect(0, 0, 160, 80, ST77XX_ORANGE);
  tft.setCursor(10, 15);
  tft.setTextColor(ST77XX_ORANGE);
  tft.print("  NFC CLONER / EMU  ");

  if (!nfcDostepne) {
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(10, 35);
    tft.print("Brak modulu PN532!");
    czekajNaWcisniecie();
    powrotDoMenu();
    return;
  }

  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 35);
  tft.print("ZESKANUJ ORYGINAL...");
  tft.setCursor(10, 60);
  tft.setTextColor(ST77XX_DARKGREY);
  tft.print("Dotknij, aby anulowac");

  uint8_t scannedUid[7] = {0};
  uint8_t uidLength      = 0;
  bool    cancelled      = false;

  czekajNaPuszczenieCaly();

  while (true) {
    if (digitalRead(TOUCH_PIN) == HIGH) {
      cancelled = true;
      czekajNaPuszczenieCaly();
      break;
    }
    if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, scannedUid, &uidLength, 40)) break;
    delay(10);
  }

  if (cancelled) { powrotDoMenu(); return; }

  tft.fillScreen(ST77XX_BLACK);
  tft.drawRect(0, 0, 160, 80, ST77XX_BLUE);
  tft.setCursor(10, 15);
  tft.setTextColor(ST77XX_BLUE);
  tft.print("  NFC CARD EMULATOR  ");
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(10, 35);
  tft.print("GOTOWY DO EMULACJI!");
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(10, 50);
  tft.print("UID: ");
  for (uint8_t i = 0; i < uidLength; i++) {
    if (scannedUid[i] < 0x10) tft.print("0");
    tft.print(scannedUid[i], HEX);
    tft.print(" ");
  }
  tft.setCursor(10, 68);
  tft.setTextColor(ST77XX_DARKGREY);
  tft.print("Dotknij, aby zakonczyc");

  uint8_t emuUid[3] = {0};
  for (uint8_t i = 0; i < min((int)uidLength, 3); i++) emuUid[i] = scannedUid[i];

  uint8_t command[] = {
    0x8C, 0x01, 0x04, 0x00,
    emuUid[0], emuUid[1], emuUid[2], 0x20,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0x00,0x00
  };

  delay(500);
  czekajNaPuszczenieCaly();

  while (digitalRead(TOUCH_PIN) == LOW) {
    nfc.sendCommandCheckAck(command, sizeof(command), 30);
    delay(10);
  }

  nfc.begin();
  nfc.SAMConfig();
  czekajNaPuszczenieCaly();
  powrotDoMenu();
}

// -------------------------------------------------------
// BLE INIT / DEINIT
// -------------------------------------------------------
void init_lightweight_ble() {
  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  if (esp_bt_controller_init(&bt_cfg) != ESP_OK)   return;
  if (esp_bt_controller_enable(ESP_BT_MODE_BLE) != ESP_OK) return;
  if (esp_bluedroid_init() != ESP_OK)               return;
  if (esp_bluedroid_enable() != ESP_OK)             return;
  esp_ble_gap_register_callback(ble_gap_cb);
}

void deinit_lightweight_ble() {
  esp_ble_gap_stop_scanning();
  delay(100);
  esp_bluedroid_disable();
  esp_bluedroid_deinit();
  esp_bt_controller_disable();
  esp_bt_controller_deinit();
}

// -------------------------------------------------------
// OPCJA 5: BRUCE BLE SCANNER
// -------------------------------------------------------
void uruchomBruceBLEScanner() {
  tft.fillScreen(ST77XX_BLACK);
  tft.drawRect(0, 0, 160, 80, ST77XX_MAGENTA);
  tft.setTextColor(ST77XX_MAGENTA);
  tft.setCursor(10, 12);
  tft.print("BRUCE: BLE SCANNER");

  init_lightweight_ble();

  esp_ble_scan_params_t scan_params = {
    .scan_type          = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval      = 0x50,
    .scan_window        = 0x30
  };
  esp_ble_gap_set_scan_params(&scan_params);
  czekajNaPuszczenieCaly();

  while (digitalRead(TOUCH_PIN) == LOW) {
    bleDevicesCount = 0;
    esp_ble_gap_start_scanning(1);
    delay(1100);

    tft.fillScreen(ST77XX_BLACK);
    tft.drawRect(0, 0, 160, 80, ST77XX_MAGENTA);
    tft.setTextColor(ST77XX_MAGENTA);
    tft.setCursor(10, 12);
    tft.printf("Aktywne cele BLE: %d", bleDevicesCount);

    for (int i = 0; i < bleDevicesCount; i++) {
      tft.setCursor(5, 26 + i * 18);
      tft.setTextColor(ST77XX_YELLOW);
      tft.printf("%s (%d)", znalezioneUrzadzenia[i].name, znalezioneUrzadzenia[i].rssi);
      tft.setCursor(5, 34 + i * 18);
      tft.setTextColor(ST77XX_DARKGREY);
      tft.print(znalezioneUrzadzenia[i].mac);
    }
  }

  deinit_lightweight_ble();
  czekajNaPuszczenieCaly();
  powrotDoMenu();
}

// -------------------------------------------------------
// OPCJA 6: BRUCE WARDRIVING (ZAKTUALIZOWANA)
// -------------------------------------------------------
void uruchomBruceWardriving() {
  tft.fillScreen(ST77XX_BLACK);
  tft.drawRect(0, 0, 160, 80, ST77XX_RED);
  tft.setTextColor(ST77XX_RED);
  tft.setCursor(10, 12);
  tft.print("BRUCE: WARDRIVING");
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 35);
  tft.print("Przeczesywanie pasma...");

  // Pasywne skanowanie z uwzględnieniem sieci ukrytych
  int n = WiFi.scanNetworks(false, true);
  
  tft.fillScreen(ST77XX_BLACK);
  tft.drawRect(0, 0, 160, 80, ST77XX_RED);
  tft.setTextColor(ST77XX_RED);
  tft.setCursor(5, 12);
  tft.printf("Zlapano AP: %d", n);

  if (n > 0) {
    int wyswietl = min(n, 3);
    for (int i = 0; i < wyswietl; i++) {
      String ssid = WiFi.SSID(i);
      if (ssid.length() == 0) {
        ssid = "[Ukryta]";
      } else if (ssid.length() > 10) {
        ssid = ssid.substring(0, 8) + "..";
      }

      String sec = konwertujZabezpieczenie(WiFi.encryptionType(i));

      tft.setCursor(5, 26 + i * 18);
      tft.setTextColor(ST77XX_WHITE);
      tft.printf("%s [Ch:%d]", ssid.c_str(), WiFi.channel(i));
      tft.setCursor(5, 34 + i * 18);
      tft.setTextColor(ST77XX_YELLOW);
      tft.printf("RSSI:%d %s", WiFi.RSSI(i), sec.c_str());
    }
  } else if (n == 0) {
    tft.setCursor(5, 35);
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Brak sieci w zasiegu.");
  }

  tft.setCursor(5, 72);
  tft.setTextColor(ST77XX_DARKGREY);
  tft.print("Dotknij, aby wyjsc");

  // Bezpieczne zwolnienie pamięci podręcznej struktur skanowania WiFi
  WiFi.scanDelete();

  czekajNaWcisniecie();
  powrotDoMenu();
}

// -------------------------------------------------------
// OPCJA 7: PASYWNY MONITOR RUCHU BLE
// -------------------------------------------------------
void uruchomMonitorRuchuBLE() {
  tft.fillScreen(ST77XX_BLACK);
  tft.drawRect(0, 0, 160, 80, ST77XX_BLUE);
  tft.setTextColor(ST77XX_BLUE);
  tft.setCursor(10, 12);
  tft.print("BLE TRAFFIC MONITOR");

  init_lightweight_ble();

  esp_ble_scan_params_t scan_params = {
    .scan_type          = BLE_SCAN_TYPE_PASSIVE,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval      = 0x50,
    .scan_window        = 0x30
  };
  esp_ble_gap_set_scan_params(&scan_params);

  pakietyStandardowe = 0;
  pakietyIBeacon     = 0;

  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 35);
  tft.print("Analiza pakietow...");
  czekajNaPuszczenieCaly();

  while (digitalRead(TOUCH_PIN) == LOW) {
    esp_ble_gap_start_scanning(1);
    delay(1050);

    tft.fillScreen(ST77XX_BLACK);
    tft.drawRect(0, 0, 160, 80, ST77XX_BLUE);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(10, 15);
    tft.print("Wychwycone pakiety:");
    tft.setCursor(10, 38);
    tft.setTextColor(ST77XX_YELLOW);
    tft.printf("Standard BLE: %d", pakietyStandardowe);
    tft.setCursor(10, 55);
    tft.setTextColor(ST77XX_MAGENTA);
    tft.printf("Tagi/Beacony: %d", pakietyIBeacon);
    tft.setCursor(10, 72);
    tft.setTextColor(ST77XX_DARKGREY);
    tft.print("Dotknij, aby wyjsc");
  }

  deinit_lightweight_ble();
  czekajNaPuszczenieCaly();
  powrotDoMenu();
}

// -------------------------------------------------------
// LOOP GŁÓWNY
// -------------------------------------------------------
void loop() {
  if (apUruchomiony && digitalRead(TOUCH_PIN) == HIGH) {
    czekajNaPuszczenieCaly();
    apUruchomiony = false;
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    powrotDoMenu();
  }

  obslugaSzybkiejRotacji();

  if (!wTrakciePracy) {
    int wartoscPot   = analogRead(POT_PIN);
    int aktualnaOpcja = map(wartoscPot, 0, 4095, 0, ILOSC_OPCJI - 1);

    if (aktualnaOpcja != wybranaOpcja) {
      obecnaPozycjaY = (aktualnaOpcja > wybranaOpcja) ? 90.0f : -20.0f;
      wybranaOpcja   = aktualnaOpcja;
    }

    if (wybranaOpcja != ostatniaOpcja) {
      tft.fillScreen(ST77XX_BLACK);
      tft.drawRect(0, 0, 160, 80, ST77XX_ORANGE);
      tft.setCursor(10, 35);
      tft.setTextColor(ST77XX_WHITE);
      tft.print(menuItems[wybranaOpcja]);
      ostatniaOpcja = wybranaOpcja;
    }
  }

  // Animacja OLED
  docelowaPozycjaY = 38.0f;
  obecnaPozycjaY  += (docelowaPozycjaY - obecnaPozycjaY) * 0.25f;

  oled.clearBuffer();
  oled.setFont(u8g2_font_04b_03_tr);
  oled.drawStr(12, 8, "NATIVE BRUCE MULTITOOL");
  oled.drawHLine(0, 11, 128);
  oled.setFont(u8g2_font_7x14_tf);
  oled.drawStr(4, (int)obecnaPozycjaY, menuItems[wybranaOpcja]);
  oled.drawHLine(0, 53, 128);
  oled.setFont(u8g2_font_04b_03_tr);
  oled.drawStr(2, 62, "STATUS: GOTOWY");
  if (nfcDostepne) oled.drawStr(95, 62, "[NFC]");
  oled.sendBuffer();

  if (!wTrakciePracy && digitalRead(TOUCH_PIN) == HIGH) {
    wTrakciePracy = true;
    switch (wybranaOpcja) {
      case 0: skanujWiFi();             break;
      case 1:                           // GENERUJ 20+ SIECI
        czekajNaPuszczenieCaly();
        WiFi.mode(WIFI_AP);
        WiFi.softAP(zabawneSieci[0], NULL, 1, 0, 4);
        apUruchomiony = true;
        break;
      case 2: uruchomRadar();          break;
      case 3: uruchomKlonerNFC();      break;
      case 4: uruchomBruceBLEScanner(); break;
      case 5: uruchomBruceWardriving(); break;
      case 6: uruchomMonitorRuchuBLE(); break;
    }
  }

  delay(15);
}