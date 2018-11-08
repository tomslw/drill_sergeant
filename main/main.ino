
#include <EthernetUdp.h>
#include <SPI.h>
#include <Ethernet.h>

byte WOL_packet[102];                                           // the WOL packet

byte mac[]{ 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };               // mac address of the ethernet module
byte PC_mac[]{ 0x60, 0xA4, 0x4C, 0x71, 0x3C, 0x30 };            // mac address of the PC thats going to be waken up

byte brodcast_address[] = { 255,255,255,255 };                  // brocast address   - might need to change later -

unsigned int localPort = 9;                                     // local port

EthernetUDP Udp;                                                // udp stuff


void setup () {
  Ethernet.begin( mac );                                        // gets an ip adress from the dhcp server
  Udp.begin(localPort);                                         // more udp stuff

  Serial.begin(9600);                   // no no
}


void loop () {
  Serial.println( Ethernet.localIP() ); //no no

  send_WOL();
  
  Ethernet.maintain();                                          // renews the dhcp address if needed

  delay( 10000 );
}


void send_WOL () {
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

  Udp.beginPacket( brodcast_address, 9 );                       // puts in the brodcast address and port on witch the packet will go out to
  Udp.write( WOL_packet, 102 );                                 // sends out the packet
  Udp.endPacket();                                              // ends packet
}
