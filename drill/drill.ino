
#include <EthernetUdp.h>
#include <SPI.h>
#include <Ethernet.h>

byte WOL_packet[102];                                           // the WOL packet

IPAddress ip(192,168,1,2); //@@@@@@@@ this is to be deleted if connected to a router

byte mac[]{ 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };               // mac address of the ethernet module
byte PC_mac[]{ 0x60, 0xA4, 0x4C, 0x71, 0x3C, 0x30 };            // mac address of the PC thats going to be waken up

byte brodcast_address[] = { 255,255,255,255 };                  // brocast address   - might need to change later -

const unsigned int localPort = 9;                               // local port

EthernetUDP Udp;                                                // udp stuff

int8_t answer;                                                  // its a temp variable to get the response from the modem
int x;                                                          // its basicly used left and right
int onModulePin = 2;                                            // pins and stuff
char temp_string[30];                                           // sent to save messages temprary
char phone_number[] = "2********";                              // The number it will send stuff to if needed
char pin[] = "2021";                                            // the pin for the sim card




int8_t sendATcommand ( char* ATcommand, char* expected_answer, unsigned int timeout ){
  uint8_t x=0,  answer=0;
  char response[100];
  unsigned long previous;
  memset( response, '\0', 100 );
  delay( 100 );
  while( Serial.available() > 0 ) Serial.read(); 
  Serial.println(ATcommand); 
  x = 0;
  previous = millis();
  do{
    if( Serial.available() != 0 ){    
      response[x] = Serial.read();
      x++;
      if ( strstr( response, expected_answer ) != NULL )    {
        answer = 1;
      }
    }
  }while( ( answer == 0 ) && ( ( millis() - previous ) < timeout ) ); 
  return answer;
}



void send_msg( char sms_text[] ) {

  while( ( sendATcommand( "AT+CREG?", "+CREG: 0,1", 500 ) || sendATcommand( "AT+CREG?", "+CREG: 0,5", 500 ) ) == 0);

  sendATcommand("AT+CMGF=1", "OK", 1000);                       // sets modem to a sms mode thingy
  sprintf( temp_string, "AT+CMGS=\"%s\"", phone_number );       // sets number to send msg to
  answer = sendATcommand( temp_string, ">", 2000 );
  
  if ( answer == 1 ) {
    Serial.println( sms_text );
    Serial.write( 0x1A );
    answer = sendATcommand( "", "OK", 20000 );
  }
}


void send_WOL() {
  Udp.beginPacket( brodcast_address, 9 );                       // puts in the brodcast address and port on witch the packet will go out to
  Udp.write( WOL_packet, 102 );                                 // sends out the packet
  Udp.endPacket();                                              // ends packet
}


void power_on() {
  uint8_t check = 0;
  
  check = sendATcommand( "AT", "OK", 2000);                     // sends AT to the modem and waits for an OK answer
  if ( check == 0 ) {
    digitalWrite( onModulePin, HIGH );                          // if no OK then it resets the modem
    delay( 3000 );
    digitalWrite( onModulePin, LOW );

    while( check == 0 ) {
      check = sendATcommand( "AT", "OK", 2000 );
    }
  }
}


void setup() {

  Ethernet.begin( mac, ip );                                    // sets mac and ip and stuff for the etherent module
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

  pinMode( onModulePin, OUTPUT );
  pinMode( 8, OUTPUT );                                         // the pin that will control the RELAY ( the relay is connected to the power button of the pc )
  Serial.begin( 115200 );
  
  power_on();
  

  delay( 3000 );

  sprintf( temp_string, "AT+CPIN=%s", pin );
  sendATcommand( temp_string, "OK", 2000 );

  delay( 10000 );

  while ( 0 == sendATcommand( "AT+CMGD=1,4", "OK", 5000 ) );

  sendATcommand( "AT+CMGF=1", "OK", 1000 );                     // sets SMS mode to stuff
  sendATcommand( "AT+CPMS=\"SM\",\"SM\",\"SM\"", "OK", 1000 );  // selects memory for sms stuff
}


void loop() {
  Ethernet.maintain();                                          // renews the dhcp address if needed
  char SMS[200];
  memset( SMS, 0, 200 );
  x = 0;
  answer = sendATcommand( "AT+CMGR=1", "+CMGR:", 2000 );
  if ( answer == 1 ) {
    answer = 0;
    while( Serial.available() == 0 );
    do{
      if( Serial.available() > 0 ) {
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
      digitalWrite( 8, HIGH );
      delay( 15000 );
      digitalWrite( 8, LOW );
    }
    
    while( Serial.available() > 0 ) Serial.read();
    while ( 0 == sendATcommand( "AT+CMGD=1,4", "OK", 5000 ) );
  }
  
  delay( 5000 );
}
