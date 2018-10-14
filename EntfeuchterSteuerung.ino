#include <EEPROM.h>

#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <RotaryEncoder.h>

// Debuglevel für print-Ausgaben (0 = Aus, 1 = Erstes Level, 2 = Zweites Level, 3 = alles)
#define DEBUG_LEVEL   3

// Wichtige Parameter:
// PINs
#define TASTER_PIN    A1          // PIN fuer Druckschalter (auf + gezogen)
#define DHT_PIN       2           // PIN fuer DHT-Daten
#define LED_PIN       13          // Nutze die eingebaute LED als Signal-LED
#define ANAUS_PIN     4           // PIN fuer Opto-Koppler: Ein-Aus
#define MODUS_PIN     3           // PIN fuer Opto-Koppler: Von "Normal" auf "Continuous"

// Zeiten (in Milli-Sekunden)
#define ZYKLUSZEIT      1000      // 1 Sekunde (Update Display)
#define LEUCHTZEIT      8000      // 8 Sekunden (Solange bleibt das Display beleuchtet)
#define MESSZEIT        300000    // 5 Minuten  (Messungsfrequenz)
#define MIN_LAUFZEIT    900000    // 15 Minuten (Wenn angeschaltet, Gerät mindestens so lange laufen lassen

#define DHTTYPE DHT22             // DHT 22  (AM2302), AM2321

#define FEUCHTE_MAX 75
#define FEUCHTE_MIN 45

// HW Objekte
DHT dht(DHT_PIN, DHTTYPE);
RotaryEncoder DrehGeber(A2, A3);
LiquidCrystal_I2C Anzeige(0x3F, 16, 2);;

// Globale Variablen
float Aktuelle_Feuchte = 0.0;
float FeuchtSchwelle = 0.0;
boolean GeraeteStatus = false;    // Initialisierung des TrocknerStatus, Anname: Geraet ist beim Einschalten "aus"
unsigned long GeraeteStartzeit = 0; // Egal bei Status Aus
unsigned long NaechsteMessung = 0;            // Zeit wann wieder mal gemessen werden soll
unsigned long Hintergrund = 0; // Zeit, wann die Hintergrundbeleuchtung ausgeschaltet wird (0 == ist aus)


void Anzeige_Feuchte(bool _Messung_laeuft = false) {
  if (DEBUG_LEVEL > 3) {
    Serial.print("Anzeige_Feuchte, Ist: ");
    Serial.print(Aktuelle_Feuchte);
    Serial.print(" Soll: ");
    Serial.println(FeuchtSchwelle);
  }
  Anzeige.clear();
  // Anzeige Zeile 1 immer
  // F: I=Ist%  S=Soll%
  // Anzeige Zeile 2 je nach Status:
  // - Messung laeuft
  // - Messung: xxx s
  // - Trocknen xxx s
  Anzeige.setCursor(0, 0);
  Anzeige.print("F: I=");
  Anzeige.print(int(Aktuelle_Feuchte));
  if (FeuchtSchwelle == FEUCHTE_MAX) {
    Anzeige.print("% *AUS*");
  } else {
    Anzeige.print("% S=");
    Anzeige.print(int(FeuchtSchwelle));
    Anzeige.print("%");
  }
  Anzeige.setCursor(0, 1);
  if (_Messung_laeuft) {
    Anzeige.print("Messung laeuft");
  } else {
    if (GeraeteStatus) {
      Anzeige.print("Trocknen ");
      unsigned long MSecs;
      MSecs = millis();
      Anzeige.print((MSecs - GeraeteStartzeit) / 1000);
      Anzeige.print("s");
    } else {
      Anzeige.print("Messung in ");
      Anzeige.print((NaechsteMessung - millis()) / 1000);
      Anzeige.print("s");
    }
  }
}

void setup() {
  // Ausgabe über Seriell
  if (DEBUG_LEVEL > 0) {
    Serial.begin(57600);
    Serial.println("Init");
  }

  EEPROM.get(0, FeuchtSchwelle);


  // Initialisierung der Ports
  // Drucktaster
  pinMode(TASTER_PIN, INPUT);
  // intene LED als Statusanzeige
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  // Pins für Opto-Koppler
  pinMode(ANAUS_PIN, OUTPUT);
  digitalWrite(ANAUS_PIN, LOW);
  pinMode(MODUS_PIN, OUTPUT);
  digitalWrite(MODUS_PIN, LOW);

  // HW Objekte initialisieren
  DrehGeber.setPosition(int(FeuchtSchwelle));

  Anzeige.begin();
  Anzeige.noBacklight();

  dht.begin();

  // Lese initial Feuchte ein:
  boolean MessErfolg = DHT_Messung(&Aktuelle_Feuchte);
}


// Zentrale Schleife, taktet mit ZYKLUSZEIT
void loop() {
  static unsigned long letzterUpdate = 0;

  // Regelmäßig das Display auf Stand bringen
  unsigned long Jetzt = millis();
  if (letzterUpdate + ZYKLUSZEIT0 < Jetzt) {
    Anzeige_Feuchte(false);
    letzterUpdate = Jetzt;
  }
  // Wenn das Licht an ist, aber die Zeit dafür abgelaufen ist, ausschalten
  if ((Hintergrund > 0) && (Hintergrund < Jetzt) && (!GeraeteStatus)) {
    Anzeige.noBacklight();
    Hintergrund = 0;
  }

  // erster Schritt: schauen, ob Knopf gedrueckt ist:
  if ( LOW == digitalRead(TASTER_PIN) ) {
    // Knopf gedrueckt, dann Licht an und eine Messung wird gestartet
    if (Hintergrund == 0)
      Anzeige.backlight();
    Hintergrund = Jetzt + LEUCHTZEIT;
    NaechsteMessung = 0;
    if (DEBUG_LEVEL > 2) {
      Serial.println("Knopf gedrückt, triggere Messung");
    }
  }

  DrehGeber.tick();
  int newPos = DrehGeber.getPosition();
  if (FeuchtSchwelle != newPos) {
    if (Hintergrund == 0)
      Anzeige.backlight();
    Hintergrund = Jetzt + LEUCHTZEIT;
    if (newPos < FEUCHTE_MIN) {
      if (DEBUG_LEVEL > 1) {
        Serial.print("Drehwert zu niedrig: ");
        Serial.println(newPos);
      }
      DrehGeber.setPosition(FeuchtSchwelle);
    } else if (newPos > FEUCHTE_MAX) {
      if (DEBUG_LEVEL > 1) {
        Serial.print("Drehwert zu hoch: ");
        Serial.println(newPos);
      }
      DrehGeber.setPosition(FeuchtSchwelle);
    } else {
      FeuchtSchwelle = newPos;
      EEPROM.put(0, FeuchtSchwelle);
      if (DEBUG_LEVEL > 0) {
        Serial.print("Schwelle geaendert: ");
        Serial.println(FeuchtSchwelle);
      }
      Anzeige_Feuchte(false);
    }
  }

  // Pruefe ob neue Messung nötig ist:
  if (Jetzt > NaechsteMessung) {
    if (DEBUG_LEVEL > 2) {
      Serial.print("Triggere Messung, Zeit jetzt: ");
      Serial.println(Jetzt / 1000);
    }
    NaechsteMessung = Jetzt + MESSZEIT;
    Messung();
    Anzeige_Feuchte(false);
  }
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
  if (DEBUG_LEVEL > 2) {
    Serial.print("Erste Messung: Feuchte=");
    Serial.println(Aktuelle_Feuchte);
  }
  Anzeige_Feuchte(true);
  delay(1500);
  MessErfolg = DHT_Messung(&Aktuelle_Feuchte);

  if (DEBUG_LEVEL > 1) {
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
    //  Schleife beenden, nach 1 Sekunde wieder messen
    NaechsteMessung = millis() + 1000;
    return;
  }
  //  2.2 Gemessener Wert trocken (mind. 2% unter FeuchtSchwelle):
  if (Aktuelle_Feuchte < FeuchtSchwelle - 2.0 ) {
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
  if ((FeuchtSchwelle == FEUCHTE_MAX) && (GeraeteStatus) ) {
    //    Abschalten, falls per Drehregler der auf MAX gedreht wird und das Gerät läuft
    if (DEBUG_LEVEL > 1) {
      Serial.println("Geraet abschalten");
    }
    AnAusSchalten();
  }
  if ((FeuchtSchwelle < FEUCHTE_MAX) && (Aktuelle_Feuchte >= FeuchtSchwelle )) {
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
  if (Hintergrund == 0)
    Anzeige.backlight();
  Hintergrund = millis() + LEUCHTZEIT;
  // Hey, etwas schalten: zeige aktuelle Werte
  Anzeige_Feuchte(false);

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
  //  *f = dht.readHumidity();
  //  delay(2000);
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

