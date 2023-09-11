//////////////////////////////////////////////////////////////////////////////
// Das Taupunkt-Lüftungssystem 
// mit dem ESP32-CAM
// 
// veröffentlicht im MAKE Special ESP32-CAM 2023
//
// Ulrich Schmerold
// 8/2023
//////////////////////////////////////////////////////////////////////////////
#define Software_version "Version: 3.87"

#define use_OLED true
#define use_Highcharts true
#define use_Datenspeicherung true
#define debugging true
#define Format_SPIFFS false

// Dieser Code benötig zwingend die folgenden Libraries:
#include <DHT.h>
#include <time.h>
#include <ESP32Time.h>
#include <WiFi.h>
#include <ESPAsyncWebSrv.h>
#include <Wire.h>
#include <GyverOLED.h>
#include <esp_task_wdt.h>

GyverOLED<SSH1106_128x64> oled;
AsyncWebServer server(80);

ESP32Time rtc;  
String stamp;  // Variable für den Zeitstempel.

//************************************** WIFI ***************************************************************
const char *ssid     = "XXXXXXXXXXX";         // Hier die eigene SSID eintragen (RouterName)
const char *password = "XXXXXXXXXXXXXXXXXXX"; // Hier das eigene WLan-Passwort einfügen

IPAddress local_IP(192, 168, 178, 110);       // Diese IP-Adrese bekommt der Taupunkt-Lüfter
IPAddress gateway (192, 168, 178, 1);         // Gateway ins Internet
IPAddress subnet  (255, 255, 255, 0);         // Subnet-Mask
IPAddress dns1    (192, 168, 178, 1);         // eigener DMS-Server
IPAddress dns2    (  8,   8,   8, 8);         // DNS-Server von Google

//************************************* Prototyping (geordet nach Karteireiter  *******************************
//-----------------  ESP32 Taupunktlüfter ---------------
float taupunkt(float t, float r); 
String make_time_stamp ();
//------------------------ Chronik -----------------------
unsigned int SPIFFS_print_space();
String wert_hinzufuegen(String, float);
void chronik_update_all();
void chronik_load_all();
String chronic_update (String , float);
bool format_SPIFFS();
//------------------------ Debugging ---------------------
void Serial_Debugging_println(String);
void Serial_Debugging_print(String);
void soft_reset(byte = 0);
void hw_wdt_disable();
void print_Reset_reason();
void print_Core_task_is_running(String);
void hw_wdt_set_timeout(byte WDT_TIMEOUT);
void delay_Timer(String Text = "" , int interval = 1000);
void list_all_SPIFFS_files();
//------------------------------ HTML -----------------------
String processor(const String& var);
//------------------------------ OLED -----------------------
void OLED_init();
void OLED_update();
void OLED_clear();
void OLED_print(String Text);
void OLED_println(String Text, byte zeile = 0, byte Spalte = 0); 
void drawSonderzeichen(byte);
void OLED_setCursor(byte Zeile , byte Spalte = 0 );
void show_Startbildschirm();
void show_Status();
//------------------------------ Wifi -------------------------
unsigned long get_Local_Time();
void WIFI_Connect();
void WiFiEvent(WiFiEvent_t event);
void WIFI_watch_Task();
void own_WIFI_reconnect();
//************************************************** Prototyping Ende *********************************

#define wdt_timeout 60                  // Time-Out-Zeit des Hardware Watchdog in Sekunden
bool chronik_is_busy = false;           // Zeigt an, dass gerade die Chronik-Daten geladen oder gespeichert werden
bool HTML_processor_is_working = false; // Zeigt an, dass gerade eine Webseite übertragen wird

int delay_interval_0 = 10000;// Dauer Sensorwerte Anzeigen
int delay_interval_1 = 7000; // Dauer Status anzeigen
int delay_interval_2 = 4000; // Dauer allgemeine Anzeigen
int delay_interval_3 = 1500; // Dauer allgemeine Anzeigen

#define RELAIPIN 2    // Anschluss des LüfterRelais
#define DHTPIN_1 14   // 13 Datenleitung für den DHT Sensor 1 (innen)
#define DHTPIN_2 4    // 15 Datenleitung für den DHT Sensor 2 (außen)

#define RELAIS_EIN LOW
#define RELAIS_AUS HIGH
bool rel;
bool fehler = true;

//************************************** Sensoren *****************************************
#define DHTTYPE_1 DHT22   // DHT 22 
#define DHTTYPE_2 DHT22   // DHT 22  
DHT dht1(DHTPIN_1, DHTTYPE_1);  //Der Innensensor wird ab jetzt mit dht1 angesprochen
DHT dht2(DHTPIN_2, DHTTYPE_2);  //Der Außensensor wird ab jetzt mit dht2 angesprochen
float t1, h1, t2, h2, Taupunkt_1, Taupunkt_2, DeltaTP;

// ***************************   Korrekturwerte der einzelnen Sensoren*****  ***************
#define Korrektur_t_1  0   // Korrekturwert Innensensor Temperatur
#define Korrektur_t_2  0   // Korrekturwert Außensensor Temperatur
#define Korrektur_h_1  0   // Korrekturwert Innensensor Luftfeuchtigkeit
#define Korrektur_h_2  0   // Korrekturwert Außensensor Luftfeuchtigkeit

//*********************** Minnimal- / Maximalwerte vom Relais *****************************
#define SCHALTmin   3.0   // minnimaler Taupuntunterschied, bei dem das Relais schaltet
#define HYSTERESE   1.0   // Abstand von Ein- und Ausschaltpunkt
#define TEMP1_min  10.0   // Minnimale Innentemperatur, bei der die Lüftung aktiviert wird
#define TEMP2_min -30.0   // Minnimale Außentemperatur, bei der die Lüftung aktiviert wird
//*****************************************************************************************

// ***************************** Variablen für die Diagramme ******************************
String Chart_temp_in = "";
String Chart_air_in  = "";
String Chart_tp_in   = "";
String Chart_temp_out= "";
String Chart_air_out = "";
String Chart_tp_out  = "";
String Chart_tp_delta= "";

//*************************** Variablen für Zeitfunktionen  ******************************
unsigned long currentTime = millis();
unsigned long previousTime = 0; 
unsigned long previousMillis = 0;



//---------------------------------------------------------------------------------------------------------------
//--------------------------------- SETUP ----- Hinweis: Diese Task läuft auf Core-1-----------------------------
//---------------------------------------------------------------------------------------------------------------

void setup() 
{ 
  Serial.begin(115200);
  Serial_Debugging_println("");
  Serial_Debugging_println("----------------- Taupunktlüfter -----------------------------"); 
  Serial_Debugging_println("----------------  "  + String(Software_version) +  " ------------------------------");
  Serial_Debugging_println("");
 
  esp_task_wdt_init(wdt_timeout, false);   // Watchdog auf 60 Sekunden stellen
  Serial_Debugging_println("Der task_wdt wurde auf " + String(wdt_timeout) + "Sekunden gesetzt");
   
  print_Reset_reason(); // Zur Fehlersuche: Warum ist der ESP gerade neu gestartet?

  OLED_init();  // OLED initialisieren
 //-------------------------------------------------   SPIFFS formatieren  --------------------------------------------------------------------------------//
 // Vor der ersten Verwendung des SPIFFS-Speichers sollte dieser Formatiert werden.                                                                        //
 // Dazu ganz oben in diesem Karteireiter die Konstante "Format_SPIFFS" auf "true" setzen und den ESP damit flashen.                                       //
 // Nachdem im Bildschirm (oder im Serielen Monitor) "Erfolgreich" angezeigt wird, "Format_SPIFFS" wieder auf false setzen und ESP wieder damit flashen.   //
                                                                                                                                                 
  if (Format_SPIFFS == true )format_SPIFFS();    // Formatieren, wenn die Konstante Format_SPIFFS auf true gesetzt wurde
 
  pinMode(RELAIPIN, INPUT); 
  if(analogRead(RELAIPIN) > 3000)  // Formatieren, wenn beim Start der RelaisPin mit VCC verbunden ist
  {                                // wenn im Display oder dem Serialmonitor Formatiere steht, kann die Verbindung wieder entfernt werden.
   int n=0;
    for( int i=0; i<5; i++)
    {
      delay(200);
      if(analogRead(RELAIPIN) > 3000) n++;
    }
    if (n==5) 
    {
      format_SPIFFS();   
    }
    delay(5000);
  }
 //--------------------------------------------------------------------------------------------------------------------------------------------------------//  
   pinMode(RELAIPIN, OUTPUT);         // Relaispin als Output definieren
  digitalWrite(RELAIPIN, RELAIS_AUS); // Relais ausschalten
 
  show_Startbildschirm();             // den Startbildschirm des Taupunktlüfters anzeigen
  //---------------------------------------------------------------------------------------------------------------------
  if ((String(ssid).substring(0, 5) == "XXXXX") or (String(password).substring(0, 5) == "XXXXX" )) { // überprüfen ob SSID und Passwort hinterlegt wurden
    OLED_clear();
    OLED_println("Bitte erst SSID und",1,0);
    OLED_println("Passwort hinterlegen!",2,0);
    OLED_println("CPU Neustart.....",5,0);
    OLED_update();
    soft_reset(5);   
  } else  WIFI_Connect();  // WIFI-Verbindung herstellen und Server starten
  //--------------------------------------------- Sensoren starten -------------------------------------------------------
  dht1.begin(); 
  dht2.begin(); 
  //---------------------------------------------- Chronik / Highcharts ---------------------------------------------------

  chronik_load_all();                 // Alle Datensätze aus dem SPIFFS-Speicher einlesen, für die Verwendung von Highcharts
}
//------------------------------------------------------------ Ende Setup  -----------------------------------------------------------------------------------


//------------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------  LOOP ---- Hinweis: Diese Task läuft auf Core-1 --------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------------------------
void loop() 
{  
   WIFI_watch_Task(); // Kontrolliert alle 60000 ms, ob WIFI noch vorhanden ist
   
   h1 = dht1.readHumidity()+Korrektur_h_1;       // Innenluftfeuchtigkeit auslesen und unter „h1“ speichern
   t1 = dht1.readTemperature()+ Korrektur_t_1;   // Innentemperatur auslesen und unter „t1“ speichern
   h2 = dht2.readHumidity()+Korrektur_h_2;       // Außenluftfeuchtigkeit auslesen und unter „h2“ speichern
   t2 = dht2.readTemperature()+ Korrektur_t_2;   // Außentemperatur auslesen und unter „t2“ speichern

  if (fehler == true)  // Prüfen, ob gültige Werte von den Sensoren kommen
  {   
    OLED_clear();
    OLED_println("Teste Sensoren..",0,0); 
    Serial_Debugging_println ("Teste Sensoren..");    
    fehler = false; 
    if (isnan(h1) || isnan(t1) || h1 > 100 || h1 < 1 || t1 < -40 || t1 > 80 )  
    {
      OLED_println("Fehler Sensor 1",2,0);
      Serial_Debugging_println("Fehler Sensor 1");
      fehler = true;
     }else {
        OLED_println("Sensor 1 in Ordnung",2,0);
        Serial_Debugging_println("Sensor 1 in Ordnung");
     } 
     OLED_update();
     delay(500);  // Zeit um das oled zu lesen
  
      if (isnan(h2) || isnan(t2) || h2 > 100 || h2 < 1 || t2 < -40 || t2  > 80)  
      {
        OLED_println("Fehler Sensor 2",3,0);
        Serial_Debugging_println("Fehler Sensor 2");
        fehler = true;
      } else {
       OLED_println("Sensor 2 in Ordnung",3,0);
        Serial_Debugging_println("Sensor 2 in Ordnung");
      }
    OLED_update();
    delay(delay_interval_3);  // Zeit um das oled zu lesen
  } 
 // if (isnan(h1) || isnan(t1) || isnan(h2) || isnan(t2)) fehler = true;
   
 if (fehler == true) {
    digitalWrite(RELAIPIN, RELAIS_AUS); // Relais ausschalten 
    OLED_println("CPU Neustart.....",5,0);
     Serial_Debugging_println("CPU Neustart.....");
    OLED_update();
    soft_reset(3);
 }

//**** Taupunkte errechnen********
char buffer_1[6];
char buffer_2[6]; 
 Taupunkt_1 = taupunkt(t1, h1);
 Taupunkt_2 = taupunkt(t2, h2);
    
  // Werteausgabe auf dem I2C-OLED
  OLED_clear();

  sprintf(buffer_1, "%.1f", t1);          // Nachkommastellen bis auf eine Stelle streichen;
  OLED_println("S1: ",0,0);
  OLED_print(buffer_1); 
   drawSonderzeichen(0);// Sonderzeichen °C
 OLED_print("C");
   drawSonderzeichen(1);// Sonderzeichen |
  OLED_print(String(h1));
  OLED_print(" %");
  Serial_Debugging_println("Sensor 1:  " + String(buffer_1) + "°C   |   "+String(h1)+" %");
  
  sprintf(buffer_1, "%.1f", t2);          // Nachkommastellen bis auf eine Stelle streichen;
  OLED_println("S2: ",1,0);
  OLED_print(buffer_1); 
   drawSonderzeichen(0);// Sonderzeichen °C
 OLED_print("C");
   drawSonderzeichen(1);// Sonderzeichen |
  OLED_print(String(h2));
  OLED_print(" %");
   Serial_Debugging_println("Sensor 2:  " + String(buffer_1) + "°C   |   "+String(h2)+" %");

   sprintf(buffer_1, "%.1f", Taupunkt_1);    // Nachkommastellen bis auf eine Stelle streichen;
   OLED_println("Taupunkt 1: ",3,0);
   OLED_print(buffer_1); 
   drawSonderzeichen(0);// Sonderzeichen °C
   OLED_print("C");
  
   sprintf(buffer_2, "%.1f", Taupunkt_2);     // Nachkommastellen bis auf eine Stelle streichen;
   OLED_println("Taupunkt 2: ",4,0);
   OLED_print(buffer_2); 
   drawSonderzeichen(0);// Sonderzeichen °C
   OLED_print("C");
   Serial_Debugging_println("Taupunkt 1: " + String(buffer_1) +" °C |  Taupunkt 2: " + String(buffer_2) +" °C");
 
  DeltaTP = Taupunkt_1 - Taupunkt_2;
  OLED_println("Delta TP: ",6,0); 
  OLED_print(String(DeltaTP));
   drawSonderzeichen(0);// Sonderzeichen °C
  OLED_print("C");
  //---------------------------------------------- Chronik aktualisieren ---------------------------
  if (use_Datenspeicherung == true) chronik_update_all();
 //------------------------------------------------ Relais schalten ----------------------------------
  if (DeltaTP > (SCHALTmin + HYSTERESE))rel = true;
  if (DeltaTP < (SCHALTmin))rel = false;
  if (t1 < TEMP1_min )rel = false;
  if (t2 < TEMP2_min )rel = false;

  if (rel == true)
  {  
    digitalWrite(RELAIPIN, RELAIS_EIN); // Relais einschalten
    Serial_Debugging_println("Lüfter EIN");
    OLED_println("Relais: L",7,0); 
    drawSonderzeichen(2);   // Sonderzeichen ü
    OLED_print("fter AN"); 
  } else {                             
    digitalWrite(RELAIPIN, RELAIS_AUS); // Relais ausschalten
    Serial_Debugging_println("Lüfter AUS");
    OLED_println("Relais: L",7,0); 
    drawSonderzeichen(2);   // Sonderzeichen ü
    OLED_print("fter AUS"); 
  }   
 OLED_update(); 
 delay(delay_interval_0); // Zeit um das oled zu lesen
 show_Status();
 delay(delay_interval_1); // Zeit um das oled zu lesen
}

float taupunkt(float t, float r) 
{  
 float a, b;
  
  if (t >= 0) {
    a = 7.5;
    b = 237.3;
  } else if (t < 0) {
    a = 7.6;
    b = 240.7;
  }
  
  // Sättigungsdampfdruck in hPa
  float sdd = 6.1078 * pow(10, (a*t)/(b+t));
  
  // Dampfdruck in hPa
  float dd = sdd * (r/100);
  
  // v-Parameter
  float v = log10(dd/6.1078);
  
  // Taupunkttemperatur (°C)
  float tt = (b*v) / (a-v);
  return  tt ;  
}

String make_time_stamp ()
{
 stamp= rtc.getTime("%d.%m.%Y %H:%M:%S");
 return stamp;
}
