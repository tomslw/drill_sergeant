
#include <Ethernet.h>

#include <avr/wdt.h>

byte WOL_packet[102];                                           // the WOL packet

IPAddress ip(192, 168, 1, 2); //@@@@@@@@ this is to be deleted if connected to a router

byte mac[] { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };              // mac address of the ethernet module
byte PC_mac[] { 0x60, 0xA4, 0x4C, 0x71, 0x3C, 0x30 };           // mac address of the PC thats going to be waken up

byte brodcast_address[] = { 255, 255, 255, 255 };               // brocast address   - might need to change later -

const unsigned int localPort = 9;                               // local port
const int udpPort = 9;

EthernetUDP Udp;                                                // udp stuff

const int onModulePin = 2;                                            // pins and stuff
const int relayPin = 8;
char temp_string[30];                                           // sent to save messages temprary
char phone_number[] = "2********";                              // The number it will send stuff to if needed
char pin[] = "2021";                                            // the pin for the sim card




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
  int8_t answer;

  while ( ( sendATcommand( "AT+CREG?", "+CREG: 0,1", 500 ) || sendATcommand( "AT+CREG?", "+CREG: 0,5", 500 ) ) == 0);

  sendATcommand("AT+CMGF=1", "OK", 1000);                       // sets modem to a sms mode thingy
  sprintf( temp_string, "AT+CMGS=\"%s\"", phone_number );       // sets number to send msg to
  answer = sendATcommand( temp_string, ">", 2000 );

  if ( answer == 1 ) {
    Serial.println( sms_text );
    Serial.write( 0x1A );
    answer = sendATcommand( "", "OK", 2000 );
  }
}


void send_WOL() {
  Udp.beginPacket( brodcast_address, udpPort );                       // puts in the brodcast address and port on witch the packet will go out to
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
  sendATcommand( temp_string, "OK", 2000 );

  while ( sendATcommand( "AT+CMGD=1,4", "OK", 2000 ) == 0 );

  sendATcommand( "AT+CMGF=1", "OK", 1000 );                     // sets SMS mode to stuff
  sendATcommand( "AT+CPMS=\"SM\",\"SM\",\"SM\"", "OK", 1000 );  // selects memory for sms stuff
}


void setup() {

  wdt_enable(WDTO_8S);

  Ethernet.begin( mac, ip );                                    // sets mac and ip and stuff for the etherent module
  Udp.begin( localPort );                                       // more udp stuff

  memset( WOL_packet, 0, 102 );                                 // sets all WOL_packets values to 0
  
  for ( int i = 0; i < 6; i++ ) {                               // sets the first six values of the packet to 0xFF
    WOL_packet[i] = 0xFF;
  }
  int o = 6;
  for ( int b = 0; b < 16; b++ ) {                              // puts in the pc's mac address 16 times
    for ( int i = 0; i < 6; i++ ) {
      WOL_packet[o] = PC_mac[i];
      o++;
    }
  }

  pinMode( onModulePin, OUTPUT );
  pinMode( relayPin, OUTPUT );                                         // the pin that will control the RELAY ( the relay is connected to the power button of the pc )
  Serial.begin( 115200 );
  
  power_on();
}


void loop() {
  int x;
  int8_t answer;
  Ethernet.maintain();                                          // renews the dhcp address if needed
  wdt_reset();
  char SMS[200];
  memset( SMS, 0, 200 );
  x = 0;
  sendATcommand( "AT", "OK", 10000 );
  answer = sendATcommand( "AT+CMGR=1", "+CMGR:", 2000 );
  if ( answer == 1 ) {
    do {
      if ( Serial.available() > 0 ) {
        SMS[x] = Serial.read();
        x++;
        if ( strstr( SMS, "OK" ) != NULL ) {
          break;
        }
      }
    } while ( true );

    if ( strstr( SMS, "BACKUP_PC_ON" ) != NULL ) {
      digitalWrite( relayPin, HIGH );
      delay( 1000 );
      digitalWrite( relayPin, LOW );
    } else if ( strstr( SMS, "PC_ON" ) != NULL ) {
      send_WOL();
      delay( 1000 );
    } else if ( strstr( SMS, "FORCE_SHUT_DOWN" ) != NULL ) {
      wdt_disable();
      digitalWrite( relayPin, HIGH );
      delay( 15000 );
      digitalWrite( relayPin, LOW );
      wdt_enable(WDTO_8S);
    }
    while ( 0 == sendATcommand( "AT+CMGD=1,4", "OK", 2000 ) );
  }
  delay( 5000 );
}
