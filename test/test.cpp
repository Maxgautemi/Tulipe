#include <Arduino.h>
#include <Wire.h>
#include <TM1637Display.h>
#include <SoftwareSerial.h>
#include "DFRobotDFPlayerMini.h"

// ========================================
// DÉFINITION DES BROCHES
// ========================================

// Broches pour l'afficheur 7 segments TM1637
#define CLK 3  // Horloge (Clock)
#define DIO 2  // Données (Data Input/Output)

// Broches I2C pour le clavier matriciel via PCF8574
#define SDA_PIN 8  // Data I2C
#define SCL_PIN 9  // Clock I2C

// Broches pour le module DFPlayer Mini (lecteur MP3)
#define DF_RX 7  // Réception (connecté au TX du DFPlayer)
#define DF_TX 6  // Transmission (connecté au RX du DFPlayer)

// ========================================
// CONSTANTES
// ========================================

#define PCF_ADDR 0x20      // Adresse I2C du PCF8574 (expandeur I/O)
#define Tulipe_Code 1590   // Code secret à deviner

// Numéros des fichiers audio sur la carte SD (0001.mp3, 0002.mp3, etc.)
#define BEEP 2  // Son de touche pressée
#define WIN  4  // Son de victoire
#define FAIL 3  // Son d'échec
#define TICK 1  // Son du tic-tac (optionnel)

// ========================================
// VARIABLES GLOBALES
// ========================================

// Tableau pour stocker les 4 chiffres entrés par l'utilisateur
unsigned long Tulipe_C_Finish[4] = {0,0,0,0};

// Compteur du nombre de chiffres entrés (0 à 4)
unsigned long compteur = 0;

// Variables pour le chronomètre dégressif
unsigned long Secondes = 30;  // Secondes restantes
unsigned long Minutes = 15;    // Minutes restantes

// Variable pour suivre le temps écoulé (décompte chaque seconde)
unsigned long Last_millis = 0;

// Flag pour savoir si le jeu est toujours actif
bool gameActive = true;

// ========================================
// INITIALISATION DES OBJETS
// ========================================

// Objet pour contrôler l'afficheur 7 segments
//TM1637Display display(CLK, DIO);

// Port série logiciel pour communiquer avec le DFPlayer
//SoftwareSerial dfSerial(DF_RX, DF_TX);

// Objet pour contrôler le lecteur MP3 DFPlayer Mini
//DFRobotDFPlayerMini myDFPlayer;

// ========================================
// CONFIGURATION DU CLAVIER MATRICIEL
// ========================================

// Matrice du clavier 4x4 (miroir vertical appliqué)
// Layout physique après correction du câblage :
//   1  2  3  A
//   4  5  6  B
//   7  8  9  C
//   *  0  #  D
const char keys[4][4] = {
  {'1','4','7','*'}, // Ligne 1 (anciennement ligne 4)
  {'2','5','8','0'}, // Ligne 2 (anciennement ligne 3)
  {'3','6','9','#'}, // Ligne 3 (anciennement ligne 2)

  };

// ========================================
// FONCTION : Gestion du chronomètre
// ========================================
void MIN_Sec(){
  // Si le jeu n'est plus actif, on arrête la mise à jour
  if (!gameActive) return;
 
  // Vérifie si une seconde s'est écoulée
  if (millis() - Last_millis >= 1000){
    Last_millis = millis();  // Mise à jour du dernier temps enregistré

    // Si le temps est déjà à zéro, on ne fait rien
    if (Minutes == 0 && Secondes == 0) return;

    // Optionnel : Jouer un tic-tac à chaque seconde
    //myDFPlayer.play(TICK);


    // Décrémenter le temps
    if (Secondes == 0) {
      if (Minutes > 0) {
        Secondes = 59;  // Reset les secondes à 59
        Minutes--;       // Décrémente les minutes
      }
    } else {
      Secondes--;  // Décrémente simplement les secondes
    }
   
    // Mise à jour de l'affichage au format MM:SS
    // 0x40 = affiche les deux points centraux (:)
    // true = affiche les zéros initiaux
    //display.showNumberDecEx(Minutes * 100 + Secondes, 0x40, true);
  }
}

// ========================================
// FONCTION : Lecture du clavier matriciel
// ========================================
char readKeypad() {
  // Balayage de chaque ligne du clavier
  for (int i = 0; i < 4; i++) {
    // Configuration du PCF8574 :
    // - Met la ligne i à LOW (active)
    // - Garde les autres lignes à HIGH
    // - P4-P7 = lignes (outputs), P0-P3 = colonnes (inputs)
    uint8_t output = ~(1 << (i + 4)) | 0x0F;
   
    // Envoie la configuration au PCF8574
    Wire.beginTransmission(PCF_ADDR);
    Wire.write(output);
    if (Wire.endTransmission() != 0) return '\0';  // Erreur de communication

    // Lit l'état des colonnes depuis le PCF8574
    Wire.requestFrom(PCF_ADDR, (uint8_t)1);
    if (Wire.available()) {
      uint8_t data = Wire.read();
      uint8_t colData = data & 0x0F;  // Masque pour lire uniquement P0-P3 (colonnes)

      // Si au moins une colonne est à LOW, une touche est pressée
      if (colData != 0x0F) {
        // Vérifie quelle colonne est active
        for (int j = 0; j < 3; j++) {
          if (!(colData & (1 << j))) {  // Colonne j est à LOW
            delay(50);  // Anti-rebond : attente pour stabiliser la lecture
            return keys[i][j];  // Retourne la touche correspondante
          }
        }
      }
    }
  }
  return '\0';  // Aucune touche pressée
}

// ========================================
// FONCTION : Setup (initialisation)
// ========================================
void setup() {
  // Initialisation du port série pour le débogage
  Serial.begin(115200);

  // Attente du port série (max 3 secondes)
  while(!Serial && millis() < 3000);

  //Serial.println("Démarrage de l'afficheur...");

  // Configuration de l'afficheur TM1637
  //display.setBrightness(0x0a);  // Luminosité (0x00 min à 0x0f max)
  //display.clear();               // Efface l'afficheur
  //Serial.println("Afficheur initialisé.");
 
  // Initialisation du bus I2C avec les broches personnalisées
  Wire.begin(SDA_PIN, SCL_PIN);
  //Serial.println("Clavier initialise : Mode Miroir Vertical");

  // Scanner I2C
  Serial.println("Scan I2C...");
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    byte err = Wire.endTransmission();
    if (err == 0) {
      Serial.print("Dispositif trouve a l'adresse 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
    }
  }
  Serial.println("Scan termine.");
}

// ========================================
// FONCTION : Loop (boucle principale)
// ========================================
void loop() {
  // Lecture du clavier
  char key = readKeypad();
 
  // Si une touche a été pressée
  if (key != '\0') {
    Serial.print("Touche detectee : ");
    Serial.println(key);
  }
}
