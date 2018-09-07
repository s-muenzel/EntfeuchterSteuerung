// Debuglevel für print-Ausgaben (0 = Aus, 1 = Erstes Level, 2 = Zweites Level, 3 = alles)
#define DEBUG_LEVEL   0

// Wichtige Parameter:
// PINs
#define TASTER_PIN    9           // PIN fuer Druckschalter (auf + gezogen)
#define DHT_PIN       10          // PIN fuer DHT-Daten
#define LED_PIN       13          // Nutze die eingebaute LED als Signal-LED
#define ANAUS_PIN     11          // PIN fuer Opto-Koppler: Ein-Aus
#define MODUS_PIN     12          // PIN fuer Opto-Koppler: Von "Normal" auf "Continuous"
#define POTI_PIN      0           // Analog-PIN mit dem Potentiometer für die Feuchtschwelleneinstellung
byte pin[7]={6,8,5,4,3,19,18};    // Byte-Array fuer die Ports der 7-Segment-Anzeige

// Zeiten (in Milli-Sekunden)
#define ZYKLUSZEIT      1000      // 1 Sekunde (Primaerschleife)
#define MESSZEIT        300000    // 5 Minuten  (Messungsfrequenz)
#define MIN_LAUFZEIT    1800000   // 30 Minuten (Wenn angeschaltet, Gerät mindestens so lange laufen lassen


#include "DHT.h"

#define DHTTYPE DHT22             // DHT 22  (AM2302), AM2321

// Connect pin 1 (on the left) of the sensor to +5V
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor

// Initialize DHT sensor.
DHT dht(DHT_PIN, DHTTYPE);




float Aktuelle_Feuchte = 0.0;
float FeuchtSchwelle = 0.0;
boolean GeraeteStatus = false;    // Initialisierung des TrocknerStatus, Anname: Geraet ist beim Einschalten "aus"
unsigned long GeraeteStartzeit = 0; // Egal bei Status Aus
unsigned long Zeit = 0;            // Zeit seit der letzten Messung in Millisekundenm kann max.5000 betragen

void setup() {
  // Ausgabe über Seriell
  if (DEBUG_LEVEL > 0) {  
    Serial.begin(9600);
  }
  if (DEBUG_LEVEL > 0) {  
    Serial.println("Init");
  }

  // Initialisierung der Ports
  // Drucktaster als Anzeigenausloeser
  pinMode(TASTER_PIN, INPUT);
  // intene LED als Statusanzeige
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  // Pins für Opto-Koppler
  pinMode(ANAUS_PIN, OUTPUT);
  digitalWrite(ANAUS_PIN, LOW);
  pinMode(MODUS_PIN, OUTPUT);
  digitalWrite(MODUS_PIN, LOW);
  // Pins für 7-Segment-Anzeige
  for (int i=0; i < 7; i++) {
    pinMode(pin[i],OUTPUT);
    digitalWrite(pin[i], HIGH);
  }

  dht.begin();

  // Lese initial Feuchte ein:
  boolean MessErfolg = DHT_Messung(&Aktuelle_Feuchte);
}


// Zentrale Schleife, taktet mit ZYKLUSZEIT
void loop() {
  Zeit += ZYKLUSZEIT;

  // erster Schritt: schauen, ob Knopf gedrueckt ist:
  if( LOW == digitalRead(TASTER_PIN) ){
    // Knopf gedrueckt
    Anzeige_Feuchte();
    // Mehr macht der Knopf nicht
    if (DEBUG_LEVEL > 2) {  
      Serial.print("Zeit seit letzter Messung jetzt: ");
      Serial.print(Zeit);
      Serial.print(" nächste Messung um: ");
      Serial.println(MESSZEIT);
    }
  }

  // zweiter Schritt: Die per Poti festgelegte Feuchtigkeitsschwelle lesen
  int PotiWert = analogRead(POTI_PIN);
  // Rechne in FeuchteSchwelle um (Min: 45.0%, Max: 75.0%)
  float F_S = 45.0 + (75.0-45.0)/1023*PotiWert;
  //Hat jemand die Schwelle um mind. 1% rel.F. veraendert?
  if (abs(FeuchtSchwelle - F_S) > 1) {
    if (DEBUG_LEVEL > 0) {  
      Serial.println("Schwelle geaendert");
    }
    FeuchtSchwelle = F_S;
    Anzeige_Feuchte(); 
    // dann auch eine Messung triggern
    Zeit = MESSZEIT+1;
  }
  
  // Pruefe ob neue Messung nötig ist:
  if (Zeit > MESSZEIT) {
    if (DEBUG_LEVEL > 2) {  
      Serial.print("Triggere Messung, Zeit jetzt: ");
      Serial.println(Zeit);
    }
    Zeit = 0;    
    Messung();
  }
  delay(ZYKLUSZEIT);
}

void Messung() {
  /* Sequenz:
    // 1. Messung
    // 2. Messung bewerten
    //  2.1 Messung fehlgeschlagen --> Schleife für 2 sek, mit 0,2 s blinkden. Schleife beenden (Sequenz wiederholen)
    //  2.2 Gemessener Wert trocken:
    //   2.2.1 Läuft Gerät, länger als MIN_LAUFZEIT
    //    2.2.1.1 Gerät ausschalten
    //  2.3 Gemessener Wert feucht:
    //   2.3.1 Läuft Gerät NICHT
    //    2.3.1.1 Gerät anschalten
  */

  // 1. Messung
  boolean MessErfolg = DHT_Messung(&Aktuelle_Feuchte);

  if(DEBUG_LEVEL>1) {
    Serial.print("Messung: Feuchte=");
    Serial.println(Aktuelle_Feuchte);
  }
  // 2. Messung bewerten
  //  2.1 Messung fehlgeschlagen?
  if (false == MessErfolg) {
    //  --> Schleife für 2,5 sek, mit 0,2 s blinken.
    boolean AnAus = false;
    if (DEBUG_LEVEL > 1) {
      Serial.println("Messung: Fehler");
    }
    // Blinken, wenn Messung schiefging
    for (int i = 0; i < 2500; i += 200) {
      if (!AnAus) {
        digitalWrite(LED_PIN, HIGH);
        AnAus = true;
      } else {
        digitalWrite(LED_PIN, LOW);
        AnAus = false;
      }
      delay(200);
    }
    //  Schleife beenden (Sequenz wiederholen) --> Trick: Zeit + MESSZEIT
    Zeit += MESSZEIT;
    return;
  }
  //  2.2 Gemessener Wert trocken (mind. 2% unter FeuchtSchwelle):
  if (Aktuelle_Feuchte < FeuchtSchwelle-2.0 ) {
    if (DEBUG_LEVEL > 1) {
      Serial.print("Feuchte < SCHWELLE:");
      Serial.println(FeuchtSchwelle);
    }
    //   2.2.1 Läuft Gerät
    if (GeraeteStatus) {
      unsigned long MSecs;
      MSecs = millis();
      if ((MSecs < GeraeteStartzeit) || (MSecs > GeraeteStartzeit + MIN_LAUFZEIT))  {
        //    2.2.1.1 Gerät ausschalten
        if (DEBUG_LEVEL > 1) {
          Serial.println("Geraet Ausschalten");
        }
        AnAusSchalten();
      }
    }
  }
  if (Aktuelle_Feuchte >= FeuchtSchwelle ) {
  //  2.3 Gemessener Wert feucht:
    //   2.3.1 Läuft Gerät NICHT
    if (DEBUG_LEVEL > 1) {
      Serial.println("Feuchte >= Schwelle");
    }
    if (!GeraeteStatus) {
      //    2.3.1.1 Gerät anschalten
      if (DEBUG_LEVEL > 1) {
        Serial.println("Geraet Anschalten");
      }
      AnAusSchalten();
    }
  }
}

void AnAusSchalten(void) {
  // Hey, etwas schalten: zeige aktuelle Werte
  Anzeige_Feuchte();

  // Einschalten: 200 ms Port ANAUS_PIN auf HIGH, dann 1 s pausieren, dann 200 ms PORT MODUS_PIN auf HIGH (1x --> "Continuous")
  // Ausschalten: 200 ms Port ANAUS_PIN auf HIGH, dann nix mehr. Aber Modus ist dann wurscht, also das gleiche wie beim Einschalten machen
  if (DEBUG_LEVEL > 2) {
    Serial.println("AnAusSchalten:");
    Serial.print("AnAus: ein");
  }
  digitalWrite(ANAUS_PIN, HIGH);
  delay(200);
  if (DEBUG_LEVEL > 2) {
    Serial.print("..aus");
  }
  digitalWrite(ANAUS_PIN, LOW);
  delay(1000);
  if (DEBUG_LEVEL > 2) {
    Serial.print("  Modus: ein");
  }
  digitalWrite(MODUS_PIN, HIGH);
  delay(200);
  if (DEBUG_LEVEL > 2) {
    Serial.println("..aus");
  }
  digitalWrite(MODUS_PIN, LOW);
  // Jetzt sollte der Modus gewechselt haben
  GeraeteStatus = !GeraeteStatus;
  // Ob Einschalten oder Ausschalten ist egal, Hauptsache ist, dass beim Einschalten die Zeit gemerkt wird.
  GeraeteStartzeit = millis();
  // Kontroll-LED auf Stand bringen
  digitalWrite(LED_PIN, GeraeteStatus ? HIGH : LOW);
  // Hey, etwas schalten: zeige aktuelle Werte
  Anzeige_Feuchte();
}

boolean DHT_Messung(float *f) {
  // Messe immer 2x, da der erste Wert der letzte gespeicherte Messwert ist.
  *f = dht.readHumidity();
  delay(2000);
  *f = dht.readHumidity();
  float t = dht.readTemperature();  // nur zur Kontrolle, keine Nutzung der Temperatur
  if (DEBUG_LEVEL > 2) {
    Serial.print("Werte: F=");
    Serial.print(*f);
    Serial.print(" T=");
    Serial.println(t);
  }
  return !( isnan(*f) || isnan(t) );
}

void Anzeige_Feuchte() {
  if (DEBUG_LEVEL > 2) {
    Serial.print("Anzeige_Feuchte, Ist: ");
    Serial.print(Aktuelle_Feuchte);
    Serial.print(" Soll: ");
    Serial.println(FeuchtSchwelle);
  }
  // Erst Ist-Wert anzeigen
  segmente(10);                       // "I"st
  segmente(int(Aktuelle_Feuchte)/10); // Zehnerstelle
  segmente(int(Aktuelle_Feuchte)%10); // Einerstelle
  segmente(13);                       // "" (Pause)
  // Jetzt Schwelle anzeigen
  segmente(11);                       // "S"oll
  segmente(int(FeuchtSchwelle)/10); // Zehnerstelle
  segmente(int(FeuchtSchwelle)%10); // Einerstelle
}

//                    0           1          2          3          4          5          6          7          8          9          I          S          -          AUS
byte SegmentBits[14]={B11111100, B01100000, B11011010, B11110010, B01100110, B10110110, B10111110, B11100000, B11111110, B11110110, B00001100, B10110110, B00000010, B00000000}; 
            
void segmente(byte n) {
  byte Bits = SegmentBits[n];
// alle 7 Segmente ansteuern
  for(int k=0; k < 7; k++) {
    if((Bits & B10000000) > 0)
      digitalWrite(pin[k], LOW); // common Anode, d.h. LOW ist an
    else 
      digitalWrite(pin[k], HIGH);
    Bits = Bits << 1;
  }
  // Anzeige: 500 ms
  delay(1000);
  // Anzeige aus (Kurz)
  for(int k=0; k < 7; k++)
      digitalWrite(pin[k], HIGH); // common Anode, d.h. LOW ist an
  delay(20);
}

