#include <WiFiS3.h>
#include "ThingSpeak.h"
#include "arduino_secrets.h"
#include <DHT11.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "RTC.h"
#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"
#include <PMS.h>  // Libreria per PMS5003

// --- Istanze e Pin ---
DHT11 dht11(2);
ArduinoLEDMatrix matrix;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600);
WiFiClient client;
PMS pms(Serial1);
PMS::DATA data;

// --- Variabili di Stato e Temporizzazione ---
unsigned long lastDHTMillis = 0;
unsigned long lastPMSCycleMillis = 0;
unsigned long pmsWarmupStartMillis = 0;
const long intervalDHT = 60 * 1000;       //  60 secondi ogni lettura DHT
const long intervalPMS = 10 * 60 * 1000;  // 15 minuti (15 * 60 * 1000) 900000
const long warmupPMS = 30 * 1000;         // 30 secondi stabilizzazione
bool dhtJustRead = false;
bool pmsFirstRead = false;

enum PMS_State { SLEEPING,
                 WARMING_UP,
                 SAMPLING };

//PMS_State pmsState = SLEEPING;
PMS_State pmsState = WARMING_UP;  // Lanciamo un campionamento PMS all'avvio

// --- Variabili Dati ---
int temp = 0, hum = 0;
float avgPM1 = 0, avgPM25 = 0, avgPM10 = 0;
long sumPM1 = 0, sumPM25 = 0, sumPM10 = 0;
int pmsSampleCount = 0;

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;
unsigned long myChannelNumber = SECRET_CH_ID;
const char *myWriteAPIKey = SECRET_WRITE_API_KEY;

// Testo lo status del canale ThingSpeak
String statusCanale = "";

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);  // Comunicazione con PMS5003

  matrix.begin();
  RTC.begin();
  ThingSpeak.begin(client);

  connectWiFi();
  syncNTP();

  ThingSpeak.setStatus(statusCanale);

  // Inizializza PMS in Sleep Mode
  //pms.sleep();
  //Serial.println("PMS5003 in modalità sleep.");
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Intervallo fra le letture DHT11 (es. 60 secondi)
  if (currentMillis - lastDHTMillis >= intervalDHT) {
    if (leggiDatiDHT(temp, hum)) {
      dhtJustRead = true;
      ThingSpeak.setField(1, temp);
      ThingSpeak.setField(2, hum);
      // Invio immediato (se il PMS non sta campionando)
      if (pmsState == SLEEPING) {
        ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
        Serial.print("Inviati a ThingSpeak temperatura ");
        Serial.print(temp);
        Serial.print(" umidità ");
        Serial.println(hum);
      } else {
        Serial.print("Valori DHT11 in attesa di invio a Thingspeak, il PMS5003 è attivo. ");
      }
    }
    lastDHTMillis = currentMillis;
  }

  // 2. Macchina a Stati PMS5003
  switch (pmsState) {
    case SLEEPING:
      if (currentMillis - lastPMSCycleMillis >= intervalPMS) {
        pms.wakeUp();
        Serial.println("PMS5003 Svegliato. Riscaldamento...");
        pmsWarmupStartMillis = currentMillis;
        pmsState = WARMING_UP;
      }
      break;

    case WARMING_UP:
      if (currentMillis - pmsWarmupStartMillis >= warmupPMS) {
        Serial.println("PMS5003 Stabilizzato. Inizio campionamento...");
        pmsSampleCount = 0;
        sumPM1 = 0;
        sumPM25 = 0;
        sumPM10 = 0;
        pmsState = SAMPLING;
      }
      break;

    case SAMPLING:
      if (pms.readUntil(data)) {
        sumPM1 += data.PM_AE_UG_1_0;
        sumPM25 += data.PM_AE_UG_2_5;
        sumPM10 += data.PM_AE_UG_10_0;
        pmsSampleCount++;
      }

      if (pmsSampleCount >= 100) {
        avgPM1 = sumPM1 / 100.0;
        avgPM25 = sumPM25 / 100.0;
        avgPM10 = sumPM10 / 100.0;

        Serial.print("PM1: ");
        Serial.print(avgPM1);
        Serial.print(" PM2.5: ");
        Serial.print(avgPM25);
        Serial.print(" PM10: ");
        Serial.println(avgPM10);


        Serial.println("Campionamento PMS completato. Invio dati...");
        inviaDatiGlobali();

        pms.sleep();
        pmsState = SLEEPING;
        lastPMSCycleMillis = currentMillis;
        
        statusCanale = "Prossimo campionamento PM fra " + String(intervalPMS) + " mSec";
        ThingSpeak.setStatus(statusCanale);
        Serial.println("PMS5003 ritorna in modalità sleep " + statusCanale);
      }
      break;
  }

  // 3. Aggiornamento Matrice LED (non bloccante)
  updateLedMatrix();
}

// --- Funzioni di Supporto ---

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

bool leggiDatiDHT(int &t, int &h) {
  int risultatoT = dht11.readTemperature();
  // Pausa necessaria per il sensore tra le due letture
  delay(1000);
  int risultatoH = dht11.readHumidity();

  // Verifica se i valori sono codici d'errore (tipicamente > 100 o costanti della libreria)
  if (risultatoT != DHT11::ERROR_CHECKSUM && risultatoT != DHT11::ERROR_TIMEOUT && risultatoH <= 100) {
    t = risultatoT;
    h = risultatoH;
    return true;
  }
  Serial.println("Errore sensore DHT: Lettura non valida");
  return false;
}

void inviaDatiGlobali() {
  ThingSpeak.setField(1, temp);
  ThingSpeak.setField(2, hum);
  ThingSpeak.setField(3, avgPM1);
  ThingSpeak.setField(4, avgPM25);
  ThingSpeak.setField(5, avgPM10);
  ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
}

void updateLedMatrix() {

  // 1. Controlla se i dati sono cambiati. Se sono uguali, non ridisegnare nulla.
  // Questo permette all'animazione SCROLL_LEFT di completarsi senza reset.
  //if (temp == lastTemp && hum == lastHum) {
  if (!dhtJustRead) {
    return;
  }

  dhtJustRead = false;
  Serial.println("Led matrix in aggiornamento...");

  matrix.beginDraw();
  matrix.textFont(Font_5x7);
  matrix.textScrollSpeed(60);
  matrix.beginText(0, 1, 0xFFFFFFFF, 0, 0);

  // Prepariamo la stringa con i dati aggiornati
  // String msg = " T:" + String(temp) + " H:" + String(hum) + "% PM2.5:" + String(avgPM25, 1);
  // matrix.println(msg);
  matrix.println(" * * *  " + getUtcDateTime() + " T: " + String(temp) + "C H:" + String(hum) + "%" + " PM10: " + String(avgPM10) + +" PM2.5: " + String(avgPM25));

  // 3. SCROLL_LEFT è bloccante: il codice si fermerà qui finché
  // la scritta non sarà passata tutta. Questo garantisce leggibilità.
  matrix.endText(SCROLL_LEFT);
  matrix.endDraw();
  // delay(5000);
  Serial.println("Led matrix aggiornata");
}

void syncNTP() {
  timeClient.begin();
  if (timeClient.update()) {
    Serial.println("Ora sincronizzata via NTP");
    RTCTime timeToSet(timeClient.getEpochTime());
    RTC.setTime(timeToSet);
    statusCanale = "RTC impostato, Arduino R4 attivo da: " + getUtcDateTime() + " UTC";
    Serial.println(statusCanale);
  }
}

void connectWiFi() {
  // if (WiFi.status() != WL_CONNECTED) {
  //   WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Tentativo di connessione a SSID: ");
    Serial.println(ssid);
    WiFi.begin(ssid, pass);
    delay(1000);
  }
  Serial.println("Connessione wifi eseguita");
}
