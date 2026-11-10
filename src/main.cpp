#include <Arduino.h>
#include <Wire.h>
#include <TM1637Display.h>
#include "DFRobotDFPlayerMini.h"

// ========================================
// DÉFINITION DES BROCHES
// ========================================

#define CLK 3
#define DIO 2

#define DF_RX 20  // RX ESP32 <- TX DFPlayer
#define DF_TX 21  // TX ESP32 -> RX DFPlayer

// ========================================
// CONSTANTES
// ========================================

#define Tulipe_Code 1590

#define BEEP 2
#define WIN  4
#define FAIL 3
#define TICK 1

// ========================================
// VARIABLES DU DÉCOMPTE
// ========================================

unsigned long START_MINUTES = 0;
unsigned long START_SECONDES = 30;

unsigned long Secondes = 0;
unsigned long Minutes = 0;
unsigned long totalSecondes = 0;

unsigned long Last_millis = 0;
unsigned long lastTick_millis = 0;

bool gameActive = true;
bool dfOk = false;  // DFPlayer détecté au démarrage

// ========================================
// INITIALISATION DES OBJETS
// ========================================

TM1637Display display(CLK, DIO);
HardwareSerial dfSerial(1);
DFRobotDFPlayerMini myDFPlayer;

// ========================================
// DECLARATION DES FONCTIONS
// ========================================

void compteur();
void afficherTemps();
void jouerSon(uint8_t piste);

// ========================================
// FONCTION : Setup
// ========================================
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);

  // Initialisation afficheur
  display.setBrightness(0x0a);
  display.clear();

  // Initialisation DFPlayer
  dfSerial.begin(9600, SERIAL_8N1, DF_RX, DF_TX);
  delay(1000);
  dfOk = myDFPlayer.begin(dfSerial);
  if (!dfOk) {
    Serial.println("DFPlayer non detecte");
  } else {
    Serial.println("DFPlayer OK");
    myDFPlayer.volume(20);
  }

  // Initialisation du décompte
  Minutes = START_MINUTES;
  Secondes = START_SECONDES;
  totalSecondes = Minutes * 60 + Secondes;

  // Affiche la valeur de départ (ex. 00:30) dès le démarrage
  afficherTemps();

  Last_millis = millis();
  lastTick_millis = millis();

  Serial.println("Décompte démarré");
}

// ========================================
// FONCTION : Loop
// ========================================
void loop() {
  if (!gameActive) return;

  compteur();
}

void compteur() {
  unsigned long now = millis();
  unsigned long quarterTime = (START_MINUTES * 60 + START_SECONDES) / 4;

  if (now - Last_millis >= 1000) {
    // += 1000 plutôt que = now : évite d'accumuler la latence de la boucle
    Last_millis += 1000;

    if (totalSecondes > 0) totalSecondes--;
    Minutes = totalSecondes / 60;
    Secondes = totalSecondes % 60;

    afficherTemps();

    Serial.print(Minutes);
    Serial.print(":");
    if (Secondes < 10) Serial.print("0");
    Serial.println(Secondes);

    if (totalSecondes == 0) {
      // Fin du temps : FAIL directement, sans TICK à 00:00
      jouerSon(FAIL);
      gameActive = false;
      Serial.println("FAIL - Temps écoulé");
    } else if (totalSecondes <= quarterTime || totalSecondes % 2 == 0) {
      // TICK : chaque seconde si <= 1/4 du temps initial, sinon toutes les 2 secondes
      jouerSon(TICK);
    }
  }
}

// ========================================
// FONCTION : Affiche Minutes:Secondes sur le TM1637
// ========================================
void afficherTemps() {
  int displayVal = (int)(Minutes * 100 + Secondes);
  display.showNumberDecEx(displayVal, 0b01000000, true);
}

// ========================================
// FONCTION : Joue une piste si le DFPlayer est présent
// (évite les blocages jusqu'à 500 ms quand le module ne répond pas)
// ========================================
void jouerSon(uint8_t piste) {
  if (dfOk) myDFPlayer.play(piste);
}