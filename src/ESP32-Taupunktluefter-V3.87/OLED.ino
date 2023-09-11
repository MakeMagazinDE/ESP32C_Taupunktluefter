//////////////////////////////////////////////////////////////////////////////
// Das Taupunkt-Lüftungssystem 
// mit dem ESP32-CAM
// 
// veröffentlicht im MAKE Special ESP32-CAM 2023
//
// Ulrich Schmerold
// 8/2023
//////////////////////////////////////////////////////////////////////////////
#define I2C_SDA 15    // 14 Pin für die i2C Schnittstelle (SDA)
#define I2C_SCL 13    // 16 Pin für die i2C Schnittstelle (SCL)


#define OLED_delay_1 100
#define OLED_delay_2 100

//***************************************** Sonderzeichen definieren ******************************************************
const static uint8_t Sonderzeichen[][8] PROGMEM = {
  {B00000000,B00000000,B00000000,B00000111,B00000101,B000111,B00000000,B00000000},   //0 Grad (°)
  {B00000000,B00000000,B11111111,B00000000,B00000000,B00000000,B00000000,B00000000}, //1 senkrechter Strich (|)
  {B00001000,B00111010,B01000000,B01000000,B00111010,B01000000,B00000000,B00000000}, //2 ü
  {B11111111,B11111111,B11111111,B11111111,B11111111,B11111111,B11111111,B00000000}  //3 Balken
 };   
//-----------------------------------------------------------------------------------------------------------

void OLED_update(){
  if (use_OLED)
  {    
    oled.update();
  }
}
//-----------------------------------------------------------------------------------------------------------

void OLED_clear()
{
  if (use_OLED)
  {  
    oled.clear(); 
  }
}
//-----------------------------------------------------------------------------------------------------------


void OLED_init(){  
  if (use_OLED)
  {  
   Wire.begin(I2C_SDA, I2C_SCL, 100000); // Mögliche Werte für Geschwindigkeit (Hz): 800000, 400000, 100000
   oled.init();
   oled.setScale(1);
   OLED_clear();
   OLED_update();
  }
}
//-----------------------------------------------------------------------------------------------------------

void OLED_setCursor(byte Zeile, byte Spalte ){
  if (use_OLED)
  {  
    oled.setCursor((Spalte*6) , Zeile); 
  }
}
//-----------------------------------------------------------------------------------------------------------

void OLED_println(String Text, byte Zeile, byte Spalte)
{
 if (use_OLED)
 {
  //    Bildschirmgröße in Zeichen:
  //    Zeile: 0 - 7
  //    Spalte 0 - 20
  
  if (Zeile>7) Zeile = 7;
  if (Spalte > 20) Spalte=20;
  if ((Spalte + Text.length()) > 20) Text.remove(21-Spalte); // remove (index) ==> vom Index bis ende String entfernen
  OLED_setCursor(Zeile, Spalte);
  oled.print(Text); 
 }
}
//-----------------------------------------------------------------------------------------------------------

void OLED_print(String Text)
{
  if (use_OLED)
  {  
    oled.print(Text); 
  }
}
//-----------------------------------------------------------------------------------------------------------


void show_Startbildschirm()
{
   Serial_Debugging_println("Zeige Startbildschirm");
   OLED_println("Taupunktl",0,3); 
    drawSonderzeichen(2);// Sonderzeichen ü
   OLED_print("fter");
   OLED_println(Software_version,1,4);
   OLED_println("by",3,9); 
   OLED_println("Ulrich Schmerold",5,2); 
   OLED_update();
   delay(delay_interval_2);  
}
//-----------------------------------------------------------------------------------------------------------

void show_Status()
{ 
  OLED_clear();
  OLED_println("- Status Seite -",0,2);
  make_time_stamp ();
  OLED_println(stamp,2,0); 
  OLED_println("SSID: "+ WiFi.SSID(),4,0);
  OLED_println("IP: "+WiFi.localIP().toString(),5,0);
   int rssi = WiFi.RSSI();
  OLED_println("RSSI: "+String(rssi)+" dBm",6,0);

 OLED_update();
}
//-----------------------------------------------------------------------------------------------------------

 void drawSonderzeichen(byte index) {
  size_t s = sizeof Sonderzeichen[index];
  for(unsigned int i = 0; i < s; i++) {
    oled.drawByte(pgm_read_byte(&(Sonderzeichen[index][i])));
  }
//-----------------------------------------------------------------------------------------------------------

  
}
