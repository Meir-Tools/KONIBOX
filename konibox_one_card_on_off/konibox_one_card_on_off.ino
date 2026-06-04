#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>

#define RST_PIN 9
#define SS_PIN 10

MFRC522 mfrc522(SS_PIN, RST_PIN);

// הגדרות EEPROM
const int ADDR_COUNT = 0;       // תא 0: כמה כרטיסים רשומים
const int ADDR_START_UIDS = 1;  // תא 1 ואילך: ה-UIDs

bool isPlaying = false; // משתנה למעקב אם מתנגן משהו
int currentPlayingCardIndex = -1; // שמירת האינדקס של הכרטיס המנגן כרגע

void setup() {
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();
  
  // אתחול מונה ה-EEPROM במקרה של ערך זבל
  if (EEPROM.read(ADDR_COUNT) > 50) {
    EEPROM.write(ADDR_COUNT, 0);
  }

  delay(500);
  printMenu();
}

void loop() {
  // 1. בדיקת תפריט ניהול (דרך ה-Serial Monitor)
  if (Serial.available() > 0) {
    handleMenu(Serial.read());
  }

  // 2. בדיקה אם נסרק כרטיס
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // 3. זיהוי הכרטיס מול ה-EEPROM
  int cardIndex = findCardIndex(mfrc522.uid.uidByte);

  if (cardIndex >= 0) {
    // אם העברנו שוב את *אותו* הכרטיס שמנגן עכשיו -> נעצור
    if (isPlaying && currentPlayingCardIndex == cardIndex) {
      Serial.print("Action: OFF (Card #");
      Serial.print(cardIndex);
      Serial.println(" toggle)");
      sendCommand(0x16, 0x00, 0x00);
      isPlaying = false;
      currentPlayingCardIndex = -1;
    } else {
      // אם זה כרטיס חדש (או שכלום לא מנגן) -> נפעיל את השיר המתאים לכרטיס (0 מפעיל 1, 1 מפעיל 2 וכו')
      Serial.print("Action: ON (Card #");
      Serial.print(cardIndex);
      Serial.println(")");
      sendCommand(0x03, 0x00, cardIndex + 1);
      isPlaying = true;
      currentPlayingCardIndex = cardIndex;
    }
  } 
  else {
    Serial.println("Unknown Card - Not in EEPROM");
  }

  mfrc522.PICC_HaltA();
  delay(1000); 
}

// --- פונקציות ניהול זיכרון ותפריט ---

void handleMenu(char choice) {
  if (choice == '1') {
    displayAllCards();
  } else if (choice == '2') {
    enrollNewCard();
  } else if (choice == '3') {
    EEPROM.write(ADDR_COUNT, 0);
    Serial.println("\n[!] Memory Reset\n");
  }
  printMenu();
}

int findCardIndex(byte* scannedUID) {
  int total = EEPROM.read(ADDR_COUNT);
  for (int i = 0; i < total; i++) {
    byte storedUID[4];
    EEPROM.get(ADDR_START_UIDS + (i * 4), storedUID);
    
    bool match = true;
    for (int j = 0; j < 4; j++) {
      if (storedUID[j] != scannedUID[j]) { match = false; break; }
    }
    if (match) return i;
  }
  return -1;
}

void enrollNewCard() {
  int count = EEPROM.read(ADDR_COUNT);
  Serial.println("Scan card to save...");
  while (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) { delay(10); }
  
  if (findCardIndex(mfrc522.uid.uidByte) != -1) {
    Serial.println("Already exists!");
  } else {
    EEPROM.put(ADDR_START_UIDS + (count * 4), mfrc522.uid.uidByte);
    EEPROM.write(ADDR_COUNT, count + 1);
    Serial.print("Saved at Index: "); Serial.println(count);
  }
}

void displayAllCards() {
  int total = EEPROM.read(ADDR_COUNT);
  Serial.print("\nRegistered: "); Serial.println(total);
  for (int i = 0; i < total; i++) {
    byte uid[4];
    EEPROM.get(ADDR_START_UIDS + (i * 4), uid);
    Serial.print("ID "); Serial.print(i);
    Serial.print(" (Song "); Serial.print(i + 1); Serial.print("): -> ");
    for(int j=0; j<4; j++) { Serial.print(uid[j], HEX); Serial.print(" "); }
    Serial.println();
  }
}

void printMenu() {
  Serial.println("\n1: List | 2: Add | 3: Clear");
}

// --- פונקציית הנגן המקורית שלך ---
void sendCommand(byte command, byte param1, byte param2) {
  byte packet[10];
  packet[0] = 0x7E; packet[1] = 0xFF; packet[2] = 0x06;
  packet[3] = command; packet[4] = 0x00;
  packet[5] = param1; packet[6] = param2;
  unsigned int checksum = -(packet[1] + packet[2] + packet[3] + packet[4] + packet[5] + packet[6]);
  packet[7] = (byte)(checksum >> 8); packet[8] = (byte)(checksum & 0xFF);
  packet[9] = 0xEF;
  Serial.write(packet, 10);
}