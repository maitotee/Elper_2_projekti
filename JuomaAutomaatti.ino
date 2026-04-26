//////////////////////////////////////////////////////////////////////////////
// Elektorniikan perusteet 2                                                //
// Päivämäärä: 29.3.2024                                                    //
// Koodissa on käytetty lähteenä:                                           //
// https://docs.arduino.cc/learn/programming/eeprom-guide/                  //
// https://forum.arduino.cc/t/stepper-motor-with-driver-a4988/405855        //
// https://github.com/miguelbalboa/rfid/tree/master                         //
// TFT-näyttö: Adafruit_ILI9341 + Adafruit_GFX kirjastot                  //
//////////////////////////////////////////////////////////////////////////////
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// RFID-pinnit
#define SS_PIN  10
#define RST_PIN  9

// Moottoriohjain
#define STEP_PIN    3
#define DIR_PIN     2
#define SLEEP_PIN   4
#define BUZZER_PIN  5

// 3.2" TFT ILI9341 - käyttää samaa SPI-väylää kuin RFID
// Vapaat pinnit (entiset LCD-pinnit):
#define TFT_CS  A0
#define TFT_DC  A1
#define TFT_RST A2

// Värit
#define TFT_BLACK   0x0000
#define TFT_WHITE   0xFFFF
#define TFT_GREEN   0x07E0
#define TFT_RED     0xF800
#define TFT_YELLOW  0xFFE0
#define TFT_BLUE    0x001F
#define TFT_CYAN    0x07FF

#define MAX_CARDS 10
#define TOTAL_DRINK_COUNT_ADDR      (MAX_CARDS * sizeof(Card))
#define EEPROM_INITIALIZED_FLAG_ADDR (TOTAL_DRINK_COUNT_ADDR + sizeof(int))
#define EEPROM_INITIALIZED_FLAG     123

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
MFRC522 mfrc522(SS_PIN, RST_PIN);

struct Card {
    char uid[10];
    char role[10];
    int drinkCount;
};

Card predefinedCards[MAX_CARDS] = {
    {"E0597219", "NIMI1", 0},
    {"E3B2C52E", "NIMI2", 0},
    {"B3258D2",  "NIMI3", 0}
};

int totalDrinkCount = 0;

// Näyttää viestin TFT-näytöllä
void tftShow(const char* rivi1, const char* rivi2, uint16_t bgColor, uint16_t txtColor) {
    tft.fillScreen(bgColor);
    tft.setTextColor(txtColor);
    tft.setTextSize(3);
    tft.setCursor(10, 80);
    tft.println(rivi1);
    if (rivi2 != nullptr) {
        tft.setCursor(10, 130);
        tft.println(rivi2);
    }
}

// Näyttää aloitusnäytön
void naytaAloitusNaytto() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_CYAN);
    tft.setTextSize(3);
    tft.setCursor(10, 60);
    tft.println("LUE KORTTI");
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, 140);
    tft.print("Shotteja: ");
    tft.println(totalDrinkCount);
}

void initializeEEPROM() {
    for (int i = 0; i < MAX_CARDS; i++) {
        int addr = i * sizeof(Card);
        EEPROM.put(addr, predefinedCards[i]);
    }
    EEPROM.put(TOTAL_DRINK_COUNT_ADDR, 0);
}

void beep(int duration) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(duration);
    digitalWrite(BUZZER_PIN, LOW);
}

void playStartupSound() {
    int tones[] = { 300, 400, 500, 700 };
    for (int i = 0; i < 4; i++) {
        tone(BUZZER_PIN, tones[i], 150);
        delay(200);
    }
    noTone(BUZZER_PIN);
}

void playHappySound() {
    int tones[] = { 700, 900, 800, 1000 };
    for (int i = 0; i < 4; i++) {
        tone(BUZZER_PIN, tones[i], 100);
        delay(150);
    }
    noTone(BUZZER_PIN);
}

bool isAuthorized(const String &uid, Card &matchedCard, int &cardIndex) {
    for (int i = 0; i < MAX_CARDS; i++) {
        int addr = i * sizeof(Card);
        Card storedCard;
        EEPROM.get(addr, storedCard);

        String storedUID = String(storedCard.uid);
        storedUID.toUpperCase();

        if (storedUID == uid) {
            matchedCard = storedCard;
            cardIndex = i;
            return true;
        }
    }
    return false;
}

void setup() {
    Serial.begin(9600);
    SPI.begin();
    mfrc522.PCD_Init();

    pinMode(STEP_PIN, OUTPUT);
    pinMode(DIR_PIN, OUTPUT);
    pinMode(SLEEP_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    digitalWrite(DIR_PIN, HIGH);
    digitalWrite(SLEEP_PIN, LOW);

    tft.begin();
    tft.setRotation(1);  // Vaakasuunta (landscape)

    playStartupSound();

    byte initialized;
    EEPROM.get(EEPROM_INITIALIZED_FLAG_ADDR, initialized);
    if (initialized != EEPROM_INITIALIZED_FLAG) {
        initializeEEPROM();
        EEPROM.put(EEPROM_INITIALIZED_FLAG_ADDR, EEPROM_INITIALIZED_FLAG);
        Serial.println("EEPROM alustettu!");
    }

    EEPROM.get(TOTAL_DRINK_COUNT_ADDR, totalDrinkCount);
    naytaAloitusNaytto();
}

void loop() {
    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
        return;
    }

    String readUID = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        readUID += String(mfrc522.uid.uidByte[i], HEX);
    }
    readUID.toUpperCase();
    // Tulostaa kortin UID:n jotta uusia kortteja on helpompi lisätä järjestelmään.
    Serial.print("Kortin UID: ");
    Serial.println(readUID);

    tftShow("Kortti", "luettu...", TFT_BLUE, TFT_WHITE);

    Card card;
    int cardIndex;

    if (isAuthorized(readUID, card, cardIndex)) {
        beep(100);

        tftShow("Hyvaksytty!", nullptr, TFT_GREEN, TFT_BLACK);
        delay(1500);

        // Näytä käyttäjänimi
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_YELLOW);
        tft.setTextSize(2);
        tft.setCursor(10, 60);
        tft.println("Kayttaja:");
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(3);
        tft.setCursor(10, 110);
        tft.println(card.role);
        delay(2000);

        // Näytä shotit
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_CYAN);
        tft.setTextSize(3);
        tft.setCursor(10, 60);
        tft.println(card.role);
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(2);
        tft.setCursor(10, 130);
        tft.print("Shotit: ");
        tft.println(card.drinkCount);
        delay(3000);

        card.drinkCount++;
        EEPROM.put(cardIndex * sizeof(Card), card);

        totalDrinkCount++;
        EEPROM.put(TOTAL_DRINK_COUNT_ADDR, totalDrinkCount);

        tftShow("Shotti", "tulossa!", TFT_BLUE, TFT_YELLOW);

        beep(200);
        runMotor(12.5);
    } else {
        for (int i = 0; i < 4; i++) {
            beep(100);
            delay(100);
        }

        tftShow("Kortti", "Hylatty!", TFT_RED, TFT_WHITE);
        delay(2000);
        naytaAloitusNaytto();
    }

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
}

void runMotor(int durationSeconds) {
    digitalWrite(SLEEP_PIN, HIGH);
    delay(10);

    unsigned long endTime = millis() + (durationSeconds * 1000);
    while (millis() < endTime) {
        digitalWrite(STEP_PIN, HIGH);
        delayMicroseconds(600);
        digitalWrite(STEP_PIN, LOW);
        delayMicroseconds(600);
    }

    tftShow("Shotti", "kaadettu!", TFT_GREEN, TFT_BLACK);
    playHappySound();

    digitalWrite(SLEEP_PIN, LOW);
    delay(2000);

    naytaAloitusNaytto();
}
