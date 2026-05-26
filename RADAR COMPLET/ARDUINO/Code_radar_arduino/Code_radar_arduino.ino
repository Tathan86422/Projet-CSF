/*******************************************************************************
 * Système Fusionné : Radar 360° Autonome & Transmission LoRaWAN (ABP)
 *******************************************************************************/

#define CFG_EU 1 // Définition de la région (Europe 868MHz)

#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <Servo.h>

// --- CONFIGURATION LORAWAN (ABP) ---
// Adresse de l'appareil (DevAddr)
static const u4_t DEVADDR = 0x260B2436;

// Clé de Session Réseau (NwkSKey)
static const PROGMEM u1_t NWKSKEY[16] = { 0xBF, 0x40, 0x6C, 0x72, 0x4E, 0x19, 0xF8, 0xE1, 0xAC, 0xA1, 0x46, 0x75, 0x82, 0x55, 0x19, 0x7A };

// Clé de Session Application (AppSKey)
static const u1_t PROGMEM APPSKEY[16] = { 0x49, 0x43, 0xAB, 0xAF, 0xCB, 0xB4, 0x7E, 0xAC, 0x1A, 0x27, 0xF2, 0x40, 0x4E, 0x4C, 0x5F, 0x72 };

// Callbacks requis pour LMIC (laissés vides pour ABP)
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }

// Variable globale pour stocker la dernière mesure à envoyer par LoRaWAN
// Format envoyé : [Angle, DistA_CM, DistB_CM]
uint8_t payloadLoRa[3] = {0, 0, 0}; 
static osjob_t sendjob;

// Intervalle de transmission LoRaWAN (en secondes)
const unsigned TX_INTERVAL = 30;

// Pin mapping pour le module LoRa (RFM95 / SX1276)
const lmic_pinmap lmic_pins = {
    .nss = 10,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 8,
    .dio = {6, 6, 6},
};

// --- CONFIGURATION RADAR ---
float propagation = 0.0343;
Servo myServo;
const int servoPin = 9;  
int angle = 0;
int stepDir = 5;         
int stepDelay = 80;      

const int trigPinA = A0;  
const int echoPinA = A1;

const int SAMPLES = 3;   

// --- FONCTIONS RADAR ---
float getDistance(int trigPin, int echoPin) {
    digitalWrite(trigPin, LOW);  
    delayMicroseconds(2);  
    digitalWrite(trigPin, HIGH);  
    delayMicroseconds(10);  
    digitalWrite(trigPin, LOW); 

    long duration = pulseIn(echoPin, HIGH, 15000); 
    
    if (duration == 0) return 0;
    return (duration * propagation) / 2;
}

void executionRadar() {
    myServo.attach(servoPin);
    myServo.write(angle);
    
    delay(stepDelay); 

    myServo.detach();
    delay(10); 

    float sumA = 0, sumB = 0;
    int validA = 0, validB = 0;

    for (int i = 0; i < SAMPLES; i++) {
        float dA = getDistance(trigPinA, echoPinA);
        if (dA > 0.1 && dA < 400.0) { sumA += dA; validA++; }
        delay(10); 
    }

    float avgA = (validA > 0) ? (sumA / validA) : -1.0;

    // Affichage local sur le port série
    Serial.print(angle);
    Serial.print(",");
    Serial.println(avgA);

    // Sauvegarde des données actuelles dans le payload pour la prochaine transmission LoRa
    payloadLoRa[0] = (uint8_t)angle;
    payloadLoRa[1] = (avgA > 0) ? (uint8_t)avgA : 0;

    angle += stepDir;

    // Gestion de la rotation du servomoteur
    if (angle >= 180) {
        angle = 180;
        stepDir = -5;  
        
        myServo.attach(servoPin);
        myServo.write(180);
        delay(150);
        myServo.detach();
        delay(100);
    }  
    else if (angle <= 0) {
        angle = 0;
        stepDir = 5;  
        
        myServo.attach(servoPin);
        myServo.write(0);
        delay(150);
        myServo.detach();
        delay(100);
    }
}

// --- FONCTIONS LORAWAN ---
void onEvent (ev_t ev) {
    switch(ev) {
        case EV_TXCOMPLETE:
            Serial.println(F("LoRaWAN: EV_TXCOMPLETE (Paquet envoyé)"));
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial.println(F("LoRaWAN: Received ack"));
            
            // Planifie la prochaine transmission LoRa
            os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
            break;
         default:
            break;
    }
}

void do_send(osjob_t* j){
    // Vérifie si une transmission LoRa est déjà en cours
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("LoRaWAN: OP_TXRXPEND, envoi reporté"));
    } else {
        // Envoie les dernières données du radar enregistrées [Angle, DistA, DistB]
        LMIC_setTxData2(1, payloadLoRa, sizeof(payloadLoRa), 0);
        Serial.println(F("LoRaWAN: Paquet radar planifié"));
    }
}

// --- SETUP & LOOP ARDUINO ---
void setup() {
    Serial.begin(115200);
    Serial.println(F("Démarrage du radar + Lora..."));
    
    // Initialisation des pins Capteurs
    pinMode(trigPinA, OUTPUT);  
    pinMode(echoPinA, INPUT); 

    // Initialisation Servo
    myServo.attach(servoPin);
    myServo.write(0); 
    delay(1000);
    myServo.detach(); 

    // Initialisation LMIC (LoRaWAN)
    os_init();
    LMIC_reset();
    LMIC_setClockError(MAX_CLOCK_ERROR * 2 / 100);

    #ifdef PROGMEM
    uint8_t appskey[sizeof(APPSKEY)];
    uint8_t nwkskey[sizeof(NWKSKEY)];
    memcpy_P(appskey, APPSKEY, sizeof(APPSKEY));
    memcpy_P(nwkskey, NWKSKEY, sizeof(NWKSKEY));
    LMIC_setSession (0x1, DEVADDR, nwkskey, appskey);
    #else
    LMIC_setSession (0x1, DEVADDR, NWKSKEY, APPSKEY);
    #endif

    #if defined(CFG_EU)
    LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      
    LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI);      
    LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      
    #endif

    LMIC_setLinkCheckMode(0);
    LMIC.dn2Dr = DR_SF9;
    LMIC_setDrTxpow(DR_SF9, 20);

    // Démarre le premier envoi LoRaWAN
    do_send(&sendjob);
}

void loop() {
    // Gère la pile LoRaWAN (non bloquant)
    os_runloop_once();
    
    // Exécute une étape du radar (balayage continu automatique)
    executionRadar();
}