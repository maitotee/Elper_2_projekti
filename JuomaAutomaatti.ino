//////////////////////////////////////////////////////////////////////////////
// Elektorniikan perusteet 2                                                //
// Päivämäärä: 29.3.2024                                                    //
// Koodissa on käytetty lähteenä:                                           //
// https://docs.arduino.cc/learn/programming/eeprom-guide/                  //
// https://forum.arduino.cc/t/stepper-motor-with-driver-a4988/405855        //
// https://github.com/miguelbalboa/rfid/tree/master                         //
//////////////////////////////////////////////////////////////////////////////
#include <SPI.h>
#include <MFRC522.h>    
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal.h>

#define SS_PIN 10
#define STEP_PIN 3
#define RST_PIN 9
#define DIR_PIN 2
#define SLEEP_PIN 4
#define BUZZER_PIN 5
#define MAX_CARDS 10

#define TOTAL_DRINK_COUNT_ADDR (MAX_CARDS * sizeof(Card))
#define EEPROM_INITIALIZED_FLAG_ADDR (TOTAL_DRINK_COUNT_ADDR + sizeof(int))
#define EEPROM_INITIALIZED_FLAG 123

LiquidCrystal lcd(A0, A1, A2, A3, A4, A5);
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

    playStartupSound();

    EEPROM.get(TOTAL_DRINK_COUNT_ADDR, totalDrinkCount);

    lcd.begin(16, 2);
    lcd.setCursor(0, 0);
    lcd.print("LUE KORTTI");
    lcd.setCursor(0, 1);
    lcd.print("Shotteja: ");
    lcd.print(totalDrinkCount);
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

    Serial.print("Kortin UID: ");
    Serial.println(readUID);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Kortti");
    lcd.setCursor(0, 1);
    lcd.print("luettu");
    delay(1500);

    Card card;
    int cardIndex;

    if (isAuthorized(readUID, card, cardIndex)) {
        beep(100);

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Hyvaksytty");
        delay(1500);

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Kayttaja:");
        lcd.setCursor(0, 1);
        lcd.print(card.role);
        delay(2000);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(card.role);
        lcd.setCursor(0, 1);
        lcd.print("shotit:");
        lcd.setCursor(8, 1);
        lcd.print(card.drinkCount);
        delay(3000);

        card.drinkCount++;
        EEPROM.put(cardIndex * sizeof(Card), card);

        totalDrinkCount++;
        EEPROM.put(TOTAL_DRINK_COUNT_ADDR, totalDrinkCount);

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Shotti tulossa!");
        lcd.setCursor(0, 1);

        beep(200);
        runMotor(11.5);
    } else {
        for (int i = 0; i < 4; i++) {
            beep(100);
            delay(100);
        }

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Tuntematon kortti");
        delay(1000);
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
        delayMicroseconds(500);
        digitalWrite(STEP_PIN, LOW);
        delayMicroseconds(500);
    }

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Shotti");
    lcd.setCursor(0, 1);
    lcd.print("kaadettu!");

    playHappySound();

    digitalWrite(SLEEP_PIN, LOW);
    delay(2000);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("LUE KORTTI");
    lcd.setCursor(0, 1);
    lcd.print("Shotteja: ");
    lcd.print(totalDrinkCount);
}