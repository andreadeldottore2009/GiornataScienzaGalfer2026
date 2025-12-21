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

  // Esempio: Leggi un sensore su A0
  //int sensorValue = random(20, 31); //analogRead(A0);

  // -----------------------

  bool erroreDht = false;

  // Attempt to read the temperature value from the DHT11 sensor.
  int temperature = dht11.readTemperature();

  // Check the result of the reading.
  // If there's no error, print the temperature value.
  // If there's an error, print the appropriate error message.
  if (temperature != DHT11::ERROR_CHECKSUM && temperature != DHT11::ERROR_TIMEOUT)
  {
    Serial.print("TEMP   | Temperatura: ");
    Serial.print(temperature);
    Serial.println(" °C");
  }
  else
  {
    Serial.print("E-DTH  | ");
    Serial.println(DHT11::getErrorString(temperature));
    erroreDht = true;
  }

  // Wait for 1 seconds before the next reading.
  delay(1000);

  // Attempt to read the humidity value from the DHT11 sensor.
  int humidity = dht11.readHumidity();

  // Check the result of the reading.
  // If there's no error, print the humidity value.
  // If there's an error, print the appropriate error message.
  if (humidity != DHT11::ERROR_CHECKSUM && humidity != DHT11::ERROR_TIMEOUT)
  {
    Serial.print("UMID   | Umidità: ");
    Serial.print(humidity);
    Serial.println(" %");
  }
  else
  {
    Serial.print("E-DTH  | ");
    Serial.println(DHT11::getErrorString(humidity));
    erroreDht = true;
  }
  

  // -----------------------
  
  if (!erroreDht)
  {
    // Imposta il valore per il Campo 1 (Field 1)
    ThingSpeak.setField(3, temperature);
  
    // Scrivi sul canale
    int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  
    if(x == 200){
      Serial.println("Canale aggiornato con successo!");
    }
    else{
      Serial.println("Errore aggiornamento. Codice HTTP: " + String(x));
    }
  }

  // ThingSpeak accetta aggiornamenti ogni 15 secondi (piano Free)
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
