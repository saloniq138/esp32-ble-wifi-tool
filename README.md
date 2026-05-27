# esp32-ble-wifi-tool
Native Bruce Multitool – ESP32 Cyber Toolkit
Native Bruce Multitool to wielofunkcyjne narzędzie diagnostyczno‑eksperymentalne oparte na ESP32, wyposażone w:

kolorowy ekran ST7735 160×80

wyświetlacz OLED 128×64

moduł PN532 NFC

skaner Wi‑Fi

skaner BLE / iBeacon

generator fałszywych sieci AP

tryby wardrivingu, trackingu i pasywnego monitoringu BLE

Projekt łączy funkcje znane z urządzeń typu Flipper Zero, Proxmark czy wardrivery, ale w lekkiej, natywnej implementacji na ESP32.

Funkcje urządzenia
1. Skaner Wi‑Fi
Wyświetla listę pobliskich sieci, RSSI oraz podstawowe informacje.
Szybki podgląd do 4 najbliższych AP.

2. Generator 20+ sieci (AP Rotation)
ESP32 tworzy Access Point i co 25 ms zmienia jego SSID na kolejne z listy zabawnych nazw.
Efekt: „burza sieci” widoczna na telefonach i laptopach.

3. Radar / Fox‑Hunt (Wi‑Fi)
Tryb polowania na wybraną sieć Wi‑Fi.
Po wyborze SSID urządzenie pokazuje:

siłę sygnału w czasie rzeczywistym

kolorowy pasek zasięgu

dynamiczną ramkę sygnału

Idealne do lokalizowania routerów lub hotspotów.

4. Kloner / Emulator NFC (PN532)
odczyt UID kart MIFARE/ISO14443A

wyświetlenie UID na ekranie

tryb emulacji UID (komenda 0x8C)

szybki restart PN532 po zakończeniu

5. Bruce BLE Scanner (z Vendor Lookup)
Skaner BLE z:

rozpoznawaniem producenta po OUI

nazwą urządzenia

RSSI

MAC

wykrywaniem pakietów iBeacon

6. Bruce Wardriving
Skaner Wi‑Fi z informacjami:

SSID

kanał

RSSI

typ zabezpieczeń

obsługa ukrytych sieci

7. Tracker / Radar BLE
Tryb polowania na wybrane urządzenie BLE:

wybór celu potencjometrem

śledzenie RSSI

kolorowy pasek sygnału

wykrywanie utraty celu

8. BLE Traffic Monitor
Pasywny analizator ruchu BLE:

liczy pakiety standardowe

liczy pakiety iBeacon / Manufacturer Data

odświeżanie co 1 sekundę

Sprzęt wymagany
ESP32‑S / ESP32‑WROOM

PN532 (I2C)

ST7735 160×80 (CS=5, DC=17, RST=16)

OLED SSD1306 128×64

potencjometr (ADC 34)

przycisk dotykowy (GPIO 13)

Struktura menu
Kod
1. Skaner Wi-Fi
2. GENERUJ 20+ SIECI
3. RADAR / Fox-Hunt
4. Klonuj / Emuluj NFC
5. Bruce: Skaner BLE
6. Bruce: Wardriving
7. Tracker / Radar BLE
8. BLE Traffic Monitor
Najważniejsze elementy kodu
Vendor Lookup – baza OUI producentów

BLE GAP Callback – analiza pakietów iBeacon

NFC Emulator – komenda 0x8C do emulacji UID

AP Rotation – ultraszybka zmiana SSID

OLED Animacja – płynne przesuwanie tekstu

Menu sterowane potencjometrem

Instalacja i kompilacja
Zainstaluj biblioteki:

U8g2lib

Adafruit_GFX

Adafruit_ST7735

Adafruit_PN532

WiFi.h

esp_wifi.h

esp_bt.h

Wgraj kod na ESP32 przez Arduino IDE.

Po uruchomieniu zobaczysz menu na OLED i ST7735.

Bezpieczeństwo i etyka
Projekt jest przeznaczony wyłącznie do celów edukacyjnych i diagnostycznych.
Nie używaj go do:

zakłócania cudzych sieci

śledzenia osób

klonowania kart, do których nie masz prawa

Plany rozwoju
zapis wardrivingu do pamięci

eksport logów BLE

tryb sniffera ESP32 (promiscuous mode)

obsługa NTAG / Mifare Classic sektorów


Schemat połączeń sprzętowych (Hardware Wiring Guide)
Poniższy schemat opisuje wszystkie połączenia między:

ESP32

ST7735 160×80 TFT

OLED SSD1306 128×64

PN532 (I2C SoftWire)

Potencjometrem

Przyciskiem dotykowym

1. Połączenia TFT ST7735 (160×80)
Tryb SPI sprzętowego.

Element TFT	Pin ESP32
CS	GPIO 5
DC	GPIO 17
RST	GPIO 16
MOSI	GPIO 23
SCK	GPIO 18
LED	3.3V (lub przez rezystor 100–330 Ω)
VCC	3.3V
GND	GND


2. Połączenia OLED SSD1306 (I2C)
Używa sprzętowego I2C ESP32.

Element OLED	Pin ESP32
SDA	GPIO 21
SCL	GPIO 22
VCC	3.3V
GND	GND


3. Połączenia PN532 (Soft I2C)
W kodzie używasz TwoWire(1) z własnymi pinami:

Element PN532	Pin ESP32
SDA	GPIO 32
SCL	GPIO 33
VCC	3.3V
GND	GND


PN532 musi być ustawiony w tryb I2C (DIP‑switche / zworki).

4. Potencjometr (ADC)
Element	Pin ESP32
Ślizgacz (środkowy)	GPIO 34
Skrajny 1	3.3V
Skrajny 2	GND


Używany do wyboru opcji w menu.

5. Przycisk dotykowy / sensor dotykowy
Element	Pin ESP32
OUT	GPIO 13
VCC	3.3V
GND	GND


W kodzie odczytywany jako wejście cyfrowe.

6. Zasilanie ESP32
USB 5V → regulator → 3.3V

Wszystkie moduły pracują na 3.3V  
(TFT, OLED, PN532, touch sensor)

ESP32
│
├── TFT ST7735 (SPI)
│   ├── CS  → GPIO 5
│   ├── DC  → GPIO 17
│   ├── RST → GPIO 16
│   ├── MOSI → GPIO 23
│   ├── SCK  → GPIO 18
│   ├── VCC → 3.3V
│   └── GND → GND
│
├── OLED SSD1306 (I2C)
│   ├── SDA → GPIO 21
│   ├── SCL → GPIO 22
│   ├── VCC → 3.3V
│   └── GND → GND
│
├── PN532 (Soft I2C)
│   ├── SDA → GPIO 32
│   ├── SCL → GPIO 33
│   ├── VCC → 3.3V
│   └── GND → GND
│
├── Potencjometr
│   ├── OUT → GPIO 34
│   ├── VCC → 3.3V
│   └── GND → GND
│
└── Przycisk dotykowy
    ├── OUT → GPIO 13
    ├── VCC → 3.3V
    └── GND → GND
