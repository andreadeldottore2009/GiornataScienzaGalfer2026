#include <WiFiS3.h>
#include "ThingSpeak.h"
#include "arduino_secrets.h"
// Include the DHT11 library for interfacing with the sensor.
#include <DHT11.h>
// Librerie per Orologio in tempo reale e sincronizzazione con server NTP internet
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "RTC.h"
// Libreria necessaria per grafica su matrcie a led
#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"


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

    ThingSpeak.setStatus(statusCanale);
}


void loop() {

  // Se il WiFi cade, riconnettiti
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  int t = 0;
  int h = 0;

  // Chiamata alla funzione: se restituisce true, i dati sono pronti
  if (leggiDatiDHT(t, h)) {
    // Carichiamo i dati nei campi di ThingSpeak
    ThingSpeak.setField(1, t);
    ThingSpeak.setField(2, h);

    // Invio unico dei dati
    int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

    if (x == 200) {
      Serial.println("Dati inviati a ThingSpeak!");
    } else {
      Serial.println("Errore ThingSpeak. Codice HTTP: " + String(x));
    }
  }

  // Visualizzazione su Matrice LED
  // Inizia la sessione di disegno sulla matrice

  matrix.beginDraw();
  matrix.textScrollSpeed(60);
  matrix.textFont(Font_5x7);  // Font integrato nella libreria

  // Imposta lo scorrimento:
  // (x iniziale, y iniziale, colore)
  matrix.beginText(0, 1, 0xFFFFFFFF, 0, 0);
  matrix.println(" * * *  " + getUtcDateTime() + " T: " + String(t) + "C H:" + String(h) + "%");

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
