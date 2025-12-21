#include <WiFiS3.h>
#include "ThingSpeak.h"
#include "arduino_secrets.h"
// Include the DHT11 library for interfacing with the sensor.
#include <DHT11.h>

// Create an instance of the DHT11 class.
// - For Arduino: Connect the sensor to Digital I/O Pin 2.
// - For ESP32: Connect the sensor to pin GPIO2 or P2.
// - For ESP8266: Connect the sensor to GPIO2 or D4.
DHT11 dht11(2);

// Credenziali da Secret Tab
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;
unsigned long myChannelNumber = SECRET_CH_ID;
const char * myWriteAPIKey = SECRET_WRITE_API_KEY;

WiFiClient  client;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  // Inizializza ThingSpeak
  ThingSpeak.begin(client);

  // Connessione WiFi
  connectWiFi();
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
