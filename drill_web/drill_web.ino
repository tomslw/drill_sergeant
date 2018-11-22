#include <EthernetUdp.h>
#include <HTTPserver.h>
#include <SPI.h>
#include <Ethernet.h>

#include <avr/wdt.h>

byte WOL_packet[102];                                           // the WOL packet

byte mac[] { 0x90, 0xA2, 0xDA, 0x00, 0x2D, 0xA1 };              // mac address of the ethernet module

byte PC_mac[6];                                                 // mac address of the PC thats going to be waken up

byte PC_mac_bak[] EEMEM = { 0x60, 0xA4, 0x4C, 0x71, 0x3C, 0x30 };

byte brodcast_address[] = { 255, 255, 255, 255 };               // brocast address   - might need to change later -

byte pin_check_bad EEMEM = 1;                               // this variable will rememer if the pin was entered corectly the first time

const unsigned int localPort = 9;                               // local port

EthernetUDP Udp;                                                // udp stuff

int8_t answer;                                                  // its a temp variable to get the response from the modem
int x;                                                          // its basicly used left and right
int onModulePin = 2;                                            // pins and stuff
char temp_string[30];                                           // sent to save messages temprary
  char pho[9];                                     // The number it will send stuff to if needed
  char pho_bak[9] EEMEM = "2********";
  char pin[5];                                            // the pin for the sim card
  char pin_bak[5] EEMEM = "2021";
int tester = 0;
EthernetServer server(80);

class myServerClass : public HTTPserver
  {
  virtual void processPostType        (const char * key, const byte flags);
  virtual void processPostArgument    (const char * key, const char * value, const byte flags);
  };

myServerClass myServer;


int8_t sendATcommand ( char* ATcommand, char* expected_answer, unsigned int timeout ) {
  uint8_t x = 0,  answer = 0;
  char response[100];
  unsigned long previous;
  memset( response, '\0', 100 );
  delay( 100 );
  while ( Serial.available() > 0 ) Serial.read();
  Serial.println(ATcommand);
  x = 0;
  previous = millis();
  do {
    if ( Serial.available() != 0 ) {
      response[x] = Serial.read();
      x++;
      if ( strstr( response, expected_answer ) != NULL )    {
        answer = 1;
      }
    }
  } while ( ( answer == 0 ) && ( ( millis() - previous ) < timeout ) );
  return answer;
}



void send_msg( char sms_text[] ) {

  while ( ( sendATcommand( "AT+CREG?", "+CREG: 0,1", 500 ) || sendATcommand( "AT+CREG?", "+CREG: 0,5", 500 ) ) == 0);

  sendATcommand("AT+CMGF=1", "OK", 1000);                       // sets modem to a sms mode thingy
  sprintf( temp_string, "AT+CMGS=\"%s\"", pho );       // sets number to send msg to
  answer = sendATcommand( temp_string, ">", 2000 );

  if ( answer == 1 ) {
    Serial.println( sms_text );
    Serial.write( 0x1A );
    answer = sendATcommand( "", "OK", 2000 );
  }
}


void send_WOL() {
  Udp.beginPacket( brodcast_address, 9 );                       // puts in the brodcast address and port on witch the packet will go out to
  Udp.write( WOL_packet, 102 );                                 // sends out the packet
  Udp.endPacket();                                              // ends packet
}


void power_on() {
  uint8_t check = 0;
  int b;
  wdt_reset();
  check = sendATcommand( "AT", "OK", 2000);                     // sends AT to the modem and waits for an OK answer
  while ( check == 0 ) {
    digitalWrite( onModulePin, HIGH );                          // if no OK then it resets the modem
    delay( 3000 );
    digitalWrite( onModulePin, LOW );
    b = 0;
    while ( check == 0 and b < 5 ) {
      check = sendATcommand( "AT", "OK", 2000 );
      b++;
    }
  }
  wdt_reset();
  delay( 3000 );

  sprintf( temp_string, "AT+CPIN=%s", pin );
  if ( sendATcommand( temp_string, "OK", 2000 ) == 0 ) {
    eeprom_update_byte( &pin_check_bad, 1 );
    return;
  }

  while ( 0 == sendATcommand( "AT+CMGD=1,4", "OK", 2000 ) );

  sendATcommand( "AT+CMGF=1", "OK", 1000 );                     // sets SMS mode to stuff
  sendATcommand( "AT+CPMS=\"SM\",\"SM\",\"SM\"", "OK", 1000 );  // selects memory for sms stuff
}


void myServerClass::processPostType (const char * key, const byte flags) {
  println(F("HTTP/1.1 200 OK"));
  println(F("Content-Type: text/html\n"
            "Connection: close\n"
            "Server: HTTPserver/1.0.0 (Arduino)"));
  println();   // end of headers
  println (F("<!DOCTYPE html>\n"
             "<html>\n"
             "<head>\n"
             "<title>Drill Sergant</title>\n"
             "<style>\n"
             "body { background-color: lightgrey; font-family:arial; }\n"
             "</style>\n"
             "</head>\n"
             "<body>\n"));
}

void myServerClass::processPostArgument (const char * key, const char * value, const byte flags)
  { 
    if ( memcmp ( key, "mac", 3 ) == 0 ) {
      sscanf(value,"%hhx-%hhx-%hhx-%hhx-%hhx-%hhx",&PC_mac[0],&PC_mac[1],&PC_mac[2],&PC_mac[3],&PC_mac[4],&PC_mac[5]);

      eeprom_update_block ( PC_mac_bak, PC_mac, 6 );
      
      memset( WOL_packet, 0, 102 );                                 // remakes the wol packet if a new mac address is asigned
      
      for ( int i = 0; i < 6; i++ ) {
        WOL_packet[1] = 0xFF;
      }
      int o = 6;
      for ( int b = 0; b < 16; b++ ) {
        for ( int i = 0; i < 6; i++ ) {
          WOL_packet[o] = PC_mac[i];
          o++;
        }
      }
    }

    if ( memcmp ( key, "phone", 5 ) == 0 ) {
      for( int i = 0; i < 8; i++ ) {
        if ( isdigit( value[i] ) ) {
          pho[i] = value[i];
        } else {
          tester = 1;
        }
      }
      pho[8] = 0x00;
      
      eeprom_update_block( pho, pho_bak, 8 );
    }
    
    if ( memcmp ( key, "newpin", 5 ) == 0 ) {                  
      for( int i = 0; i < 4; i++ ) {
        if ( isdigit( value[i] ) ) {
          pin[i] = value[i];
        } else {
          tester = 1;
        }
      }
      pin[4] = 0x00;
      
      eeprom_update_byte( &pin_check_bad, 0 );
      eeprom_update_block( pin, pin_bak, 5 );
    }
    if ( memcmp ( key, "pc_on", 5 ) == 0 ) {
      send_WOL();
    }
    if ( memcmp ( key, "force_shut_down", 15 ) == 0 ){
      wdt_disable();
      digitalWrite( 8, HIGH );
      delay( 15000 );
      digitalWrite( 8, LOW );
      wdt_enable(WDTO_8S);
    }
  }


void setup() {

  wdt_enable(WDTO_8S);

  Ethernet.begin( mac );                                    // sets mac and ip and stuff for the etherent module
  Udp.begin( localPort );                                       // more udp stuff

  memset( WOL_packet, 0, 102 );                                 // sets all WOL_packets values to 0
  
  for ( int i = 0; i < 6; i++ ) {                               // sets the first six values of the packet to 0xFF
    WOL_packet[1] = 0xFF;
  }
  int o = 6;
  for ( int b = 0; b < 16; b++ ) {                              // puts in the pc's mac address 16 times
    for ( int i = 0; i < 6; i++ ) {
      WOL_packet[o] = PC_mac[i];
      o++;
    }
  }

  eeprom_read_block( PC_mac, PC_mac_bak, 6 );

  eeprom_read_block( pho, pho_bak, 8 );

  eeprom_read_block( pin, pin_bak, 4 );
  pin[4] = 0x00;
  

  pinMode( onModulePin, OUTPUT );
  pinMode( 8, OUTPUT );                                         // the pin that will control the RELAY ( the relay is connected to the power button of the pc )
  Serial.begin( 115200 );
  
   if ( eeprom_read_byte( &pin_check_bad ) == 0 ) power_on();
}


void loop() {
  Serial.println(pin);
  static unsigned long modem_timer;
  unsigned long clok = millis();
  wdt_reset();
  Ethernet.maintain();                                          // renews the dhcp address if needed
  EthernetClient client = server.available();
  
  if ( !client && eeprom_read_byte( &pin_check_bad ) == 0 ) {
    if ( modem_timer < clok ) {
      wdt_reset();
      char SMS[200];
      memset( SMS, 0, 200 );
      x = 0;
      sendATcommand( "AT", "OK", 10000 );
      answer = sendATcommand( "AT+CMGR=1", "+CMGR:", 2000 );
      if ( answer == 1 ) {
        answer = 0;
        while ( Serial.available() == 0 );
        do {
          if ( Serial.available() > 0 ) {
            SMS[x] = Serial.read();
            x++;
            if ( strstr( SMS, "OK" ) != NULL ) {
              answer = 1;
            }
          }
        } while ( answer == 0 );
    
        if ( strstr( SMS, "BACKUP_PC_ON" ) != NULL ) {
          digitalWrite( 8, HIGH );
          delay( 1000 );
          digitalWrite( 8, LOW );
        } else if ( strstr( SMS, "PC_ON" ) != NULL ) {
          send_WOL();
          delay( 1000 );
        } else if ( strstr( SMS, "FORCE_SHUT_DOWN" ) != NULL ) {
          wdt_disable();
          digitalWrite( 8, HIGH );
          delay( 15000 );
          digitalWrite( 8, LOW );
          wdt_enable(WDTO_8S);
        }
        while ( 0 == sendATcommand( "AT+CMGD=1,4", "OK", 2000 ) );
        modem_timer = clok + 5000;
      }
    }
  } else {
    char temp[18];
    myServer.begin (&client);
    while ( client.connected() && !myServer.done ) {
      while ( client.available () > 0 && !myServer.done )
        myServer.processIncomingByte ( client.read() );
    }
    if ( tester != 1 && eeprom_read_byte( &pin_check_bad ) == 0 ) {
      myServer.print(F("<h1> Drill sergant </h1> <form method=\"post\"> New mac address to which send the WOL packet:<br> <input type=\"text\" name=\"macaddress\" value=\""));
    
      sprintf( temp, "%X-%X-%X-%X-%X-%X", PC_mac[0], PC_mac[1], PC_mac[2], PC_mac[3], PC_mac[4], PC_mac[5] );
      myServer.print( temp );
      
      myServer.print(F("\"> <br> <br> New phone number to which send updates: <br> <input type=\"text\" name=\"phonenumber\" value=\" "));
      for ( int i = 0; i < 8; i++ ) {
        myServer.print( pho[i] );
      } 
      myServer.print(F("\"> <br> <input type=submit name=Submit value=\"Process\" > </form>"));
      myServer.print(F("<br> <form method=\"post\"><input type=\"hidden\" name=\"pc_on\" value=\"on\"> <input type=\"submit\" value=\"Turn on PC\"></form><form method=\"post\">"));
      myServer.print(F("<input type=\"hidden\" name=\"force_shut_down\" value=\"off\"> <input type=\"submit\" value=\"Force shut down\"> <br>"));
      myServer.print(F("<p> Status: </p> </body> </html>"));

    } else if ( eeprom_read_byte( &pin_check_bad ) != 0 && tester != 1 ) {
      myServer.print(F("<h1> Sim pin ERROR </h1>"));
      myServer.print(F("<br><form method=\"post\"> Enter new SIM PIN: <br> <input type=\"text\" name=\"newpin\" value=\""));

      for ( int i = 0; i < 4; i++ ) {
        myServer.print( pin[i] );
      } 
      
      myServer.print(F("\"> <br> <input type=\"submit\" value=\"Submit\"> </form> </body> </html>"));
        
    } else {
      tester = 0;
      myServer.print(F("<h1>403 you put in stuff wrong</h1>"));
      myServer.print(F("</body> </html>"));
    }  
      myServer.flush();
    
      delay(1);
    
      client.stop();
  }
}
