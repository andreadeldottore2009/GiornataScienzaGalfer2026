#include <WiFiS3.h>
#include "ThingSpeak.h"
#include "arduino_secrets.h"
// Include the DHT11 library for interfacing with the sensor.
#include <DHT11.h>
// Librerie per Orologio in tempo reale e sincronizzazione con server NTP internet
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "RTC.h"
// Libreria necessaria per grafica su matrice a led
#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"
#include <PMS.h>  // Libreria per PMS5003

// Usiamo Serial1 (Pin 0 e 1 dell'Arduino R4)
PMS pms(Serial1);
PMS::DATA data;

// variabili per il campionamento del particolato
float avgPM1 = 0, avgPM25 = 0, avgPM10 = 0;
long sumPM1 = 0, sumPM25 = 0, sumPM10 = 0;
int pmsSampleCount = 0;

// --- Variabili di Stato e Temporizzazione ---
unsigned long lastDHTMillis = 0;
unsigned long lastPMSCycleMillis = 0;

const long intervalDHT = 30000;          // 60 secondi fra le letture DHT
const long intervalPMS = 2 * 60 * 1000;  // n minuti (n * 60 * 1000)
const long warmupPMS = 30000;            // 30 secondi stabilizzazione
bool isFirstRun = false;                  // Per lanciare il campionamento PMS all'avvio

// Create an instance of the DHT11 class.
// - For Arduino: Connect the sensor to Digital I/O Pin 2.
// - For ESP32: Connect the sensor to pin GPIO2 or P2.
// - For ESP8266: Connect the sensor to GPIO2 or D4.
DHT11 dht11(2);

// Matrice Led
ArduinoLEDMatrix matrix;

// Credenziali da Secret Tab
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

// --- Configurazione NTP ---
WiFiUDP ntpUDP;
// "pool.ntp.org" è il server standard. 3600 è l'offset per l'Italia (UTC+1)
// In estate usa 7200 (UTC+2) per l'ora legale.
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600);

// Canale e API Key ThingSpeak
unsigned long myChannelNumber = SECRET_CH_ID;
const char *myWriteAPIKey = SECRET_WRITE_API_KEY;

WiFiClient client;

// Variabile per lo status del canale ThingSpeak
String statusCanale = "";

void setup() {

  Serial1.begin(9600);  // Comunicazione con PMS5003
  Serial.begin(9600);
  while (!Serial)
    ;


  // Inizializza ThingSpeak
  ThingSpeak.begin(client);

  // Inizializza Led
  matrix.begin();

  // Inizializza l'orologio interno
  RTC.begin();

  // Connessione WiFi
  connectWiFi();

  // Sincronizzazione orario con server NTP

  timeClient.begin();
  if (timeClient.update()) {
    Serial.println("Ora sincronizzata via NTP.");

    // Ottieni l'ora dal server
    unsigned long epochTime = timeClient.getEpochTime();
    RTCTime timeToSet(epochTime);

    // Imposta l'RTC interno
    RTC.setTime(timeToSet);
    statusCanale = "RTC impostato, Arduino R4 attivo da: " + getUtcDateTime() + " UTC";
    Serial.println(statusCanale);
  } else {
    statusCanale = "Errore sincronizzazione NTP.";
    Serial.println(statusCanale);
  }

  // Inizializza PMS in Sleep Mode
  pms.sleep();
  Serial.println("PMS5003 in modalità sleep.");


  ThingSpeak.setStatus(statusCanale);
}

void loop() {

  // Se il WiFi cade, riconnettiti
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  unsigned long currentMillis = millis();  // Per tenere conto degli intervalli fra i campionamenti

  // PMS5003

  if (isFirstRun || (currentMillis - lastPMSCycleMillis >= intervalPMS)) {
    isFirstRun = false;
    pms.wakeUp();
    Serial.println("PMS5003 Attivato, in fase di riscaldamento...");
    delay(warmupPMS);
    Serial.println("PMS5003 Stabilizzato. Inizio ciclo di campionamento...");

    // 1. Definisci la costante a inizio file (fuori dal loop)
    const int NumberOfSamplingToDo = 50;

    // --- Loop interno della logica di campionamento

    // Reset delle variabili di accumulo
    sumPM1 = 0;
    sumPM25 = 0;
    sumPM10 = 0;
    pmsSampleCount = 0;

    // Ciclo di campionamento
    while (pmsSampleCount < NumberOfSamplingToDo) {
      if (pms.readUntil(data)) {
        sumPM1 += data.PM_AE_UG_1_0;
        sumPM25 += data.PM_AE_UG_2_5;
        sumPM10 += data.PM_AE_UG_10_0;

        pmsSampleCount++;

        // Stampa del progresso per monitorare l'avanzamento sulla Serial
        Serial.print("Campionamento PMS: ");
        Serial.print(pmsSampleCount);
        Serial.print("/");
        Serial.println(NumberOfSamplingToDo);
      }
    }



    // 2. Calcolo della media finale usando la costante
    // Usiamo (float) per assicurarci che la divisione non tronchi i decimali
    avgPM1 = (float)sumPM1 / NumberOfSamplingToDo;
    avgPM25 = (float)sumPM25 / NumberOfSamplingToDo;
    avgPM10 = (float)sumPM10 / NumberOfSamplingToDo;

    Serial.print("PM1: ");
    Serial.print(avgPM1);
    Serial.print("PM2.5: ");
    Serial.print(avgPM25);
    Serial.print("PM10: ");
    Serial.println(avgPM10);


    pms.sleep();
    Serial.println("PMS5003 disattivato, in attesa prossimo campionamento...");
    lastPMSCycleMillis = currentMillis;
  } else {
    Serial.print("Prossimo campionamento PM fra ");
    Serial.print(intervalPMS - currentMillis - lastPMSCycleMillis);
    Serial.println(" mSec");
  }

  ThingSpeak.setField(3, avgPM1);
  ThingSpeak.setField(4, avgPM25);
  ThingSpeak.setField(5, avgPM10);

  // DHT

  int t = 0;
  int h = 0;

  // if (currentMillis - lastDHTMillis >= intervalDHT) {

  Serial.println("Campionamento DHT...");
  // Chiamata alla funzione: se restituisce true, i dati sono pronti
  if (leggiDatiDHT(t, h)) {
    // Carichiamo i dati nei campi di ThingSpeak
    ThingSpeak.setField(1, t);
    ThingSpeak.setField(2, h);
  }

  // lastDHTMillis = currentMillis;
  // }

  // Invio unico dei dati
  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

  if (x == 200) {
    Serial.println("Dati inviati a ThingSpeak!");
  } else {
    Serial.println("Errore ThingSpeak. Codice HTTP: " + String(x));
  }


  // Visualizzazione su Matrice LED
  // Inizia la sessione di disegno sulla matrice

  matrix.beginDraw();
  matrix.textScrollSpeed(60);
  matrix.textFont(Font_5x7);  // Font integrato nella libreria

  // Imposta lo scorrimento:
  // (x iniziale, y iniziale, colore)
  matrix.beginText(0, 1, 0xFFFFFFFF, 0, 0);
  matrix.println(" * * *  " + getUtcDateTime() + " T: " + String(t) + "C H:" + String(h) + "%" + " PM10: " + String(avgPM10) + +" PM2.5: " + String(avgPM25));

  // Applica lo scorrimento a sinistra
  matrix.endText(SCROLL_LEFT);

  matrix.endDraw();

  // Pausa di 15 secondi richiesta da ThingSpeak (piano gratuito)
  delay(15000);
}

void connectWiFi() {
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Tentativo di connessione a SSID: ");
    Serial.println(ssid);
    WiFi.begin(ssid, pass);
    delay(5000);
  }
  Serial.println("\nConnesso.");
}

/*
 * Ritorna una stringa di data e ora formattatacome DD/MM/YYYY HH:MM:SS
 */
String getUtcDateTime() {
  RTCTime now;
  RTC.getTime(now);

  // Estrazione Data
  int giorno = now.getDayOfMonth();
  int mese = (int)now.getMonth() + 1;  // I mesi in RTC.h partono da 0 (Gennaio)
  int anno = now.getYear();

  // Estrazione Ora
  int ore = now.getHour();
  int minuti = now.getMinutes();
  int secondi = now.getSeconds();

  // Formattazione con zero iniziale (Padding)
  String sGiorno = (giorno < 10) ? "0" + String(giorno) : String(giorno);
  String sMese = (mese < 10) ? "0" + String(mese) : String(mese);
  String sOre = (ore < 10) ? "0" + String(ore) : String(ore);
  String sMinuti = (minuti < 10) ? "0" + String(minuti) : String(minuti);
  String sSecondi = (secondi < 10) ? "0" + String(secondi) : String(secondi);

  // Ritorna il formato completo
  return sGiorno + "/" + sMese + "/" + String(anno) + " " + sOre + ":" + sMinuti + ":" + sSecondi;
}

// Funzione per leggere il sensore DHT11 con pausa di sincronizzazione
bool leggiDatiDHT(int &temp, int &umid) {
  // 1. Leggi la temperatura
  temp = dht11.readTemperature();

  // 2. Pausa necessaria per il sensore tra le due letture
  delay(1000);

  // 3. Leggi l'umidità
  umid = dht11.readHumidity();

  // Verifica se ci sono stati errori nella temperatura
  if (temp == DHT11::ERROR_CHECKSUM || temp == DHT11::ERROR_TIMEOUT) {
    Serial.print("E-DTH Temp | ");
    Serial.println(DHT11::getErrorString(temp));
    return false;
  }

  // Verifica se ci sono stati errori nell'umidità
  if (umid == DHT11::ERROR_CHECKSUM || umid == DHT11::ERROR_TIMEOUT) {
    Serial.print("E-DTH Umid | ");
    Serial.println(DHT11::getErrorString(umid));
    return false;
  }

  // Se tutto è andato bene, stampa i valori per debug
  Serial.print("Lettura OK | Temp: ");
  Serial.print(temp);
  Serial.print("°C, Umid: ");
  Serial.print(umid);
  Serial.println("%");

  return true;
}
