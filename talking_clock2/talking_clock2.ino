/*
 * SD talking clock
 *
 * Created: 5/06/2017 23:34:47
 *  Author: moty22.co.uk
 */ 

#include <LiquidCrystal.h>
#include <SPI.h>
#include <SD.h>

    // initialize the library with the numbers of the interface pins
LiquidCrystal lcd(7, 8, A0, A1, A2, A3);

const int CS = 10;          // SD CS connected to digital pin PD4
const int mosi = 11;
const int clk = 13;
const int miso = 12;          //
const int audio = 6;     //output D6
const int oc2b = 3;     //1000Hz
const int t1 = 5;     //1000Hz
const int talk = 2;     //talk pushbutton
const int minutes = A4;     //minutes advance pushbutton
const int hours = A5;     //hours pushbutton
int talkPB=1;         // talk pushbutton status
int minutesPB=1;         // minutes pushbutton status
int hoursPB=1;         // hours pushbutton status

unsigned long loc,BootSector, RootDir, SectorsPerFat, RootDirCluster, DataSector, FileCluster, FileSize;  //
unsigned int BytesPerSector, ReservedSectors, card;
unsigned char fn=1, sdhc=1, SectorsPerCluster, Fats;
unsigned char minute=0, hour=0, Htalk=0;

void setup() {
pinMode(CS, OUTPUT);
pinMode(mosi, OUTPUT);
pinMode(clk, OUTPUT);
pinMode(miso, INPUT);
pinMode(talk, INPUT_PULLUP);
pinMode(minutes, INPUT_PULLUP);
pinMode(hours, INPUT_PULLUP);

lcd.begin(16, 2);   // set up the LCD's number of columns and rows:
    Serial.begin(9600);

  SD.begin(10);

    //PWM Timer0
  OCR0A = 0;
  TCCR0A = _BV(COM0A1) | _BV(WGM01) | _BV(WGM00);  //output in phase, fast PWM mode
  TCCR0B = _BV(CS00); // 16MHz/256 = 62.5KHz
  pinMode(audio, OUTPUT);
  
    //1000 Hz - timer2
  OCR2A =249;
  OCR2B = 125;  
  TCCR2A = _BV(COM2B1) | _BV(COM2B0) | _BV(WGM21) | _BV(WGM20);  //output B in phase, fast PWM mode
  TCCR2B = _BV(WGM22) | _BV(CS22); // set prescaler to 64 and start the timer
  pinMode(oc2b, OUTPUT);
  
      //60 sec - timer1
  // OCR1A = 0xEA5F;
  TCCR1A = _BV(WGM10) | _BV(WGM11) | _BV(COM1A0); //   
  TCCR1B = _BV(WGM12) | _BV(WGM13) | _BV(CS11) | _BV(CS12);  // input T1, PWM mode
  
  digitalWrite(CS, HIGH);  

  if(sdhc){card = 1;}else{card=512;}  //SD or SDHC
  fat();
  if(BytesPerSector!=512){error();}

}


void loop()
{
   if(digitalRead(4)){Htalk=0;} 
   talkPB=digitalRead(talk);
   if(talkPB==LOW || Htalk)
   { 
     Htalk = 0;
     Serial.print(hour);  Serial.print(":");   Serial.println(minute);    
            //read hours
        if(hour < 21) {fn=hour; play();}
        else if(hour > 20) {fn=20; play(); fn=hour-20; play();}
        fn=24; play();     //read "hours"      

          //read minutes
        if(minute < 21) {fn=minute; play();}
        else if(minute > 20 && minute < 30) {fn=20; play(); fn=minute-20; play();}
        else if(minute == 30) {fn=21; play();}
        else if(minute > 30 && minute < 40) {fn=21; play(); fn=minute-30; play();}
        else if(minute == 40) {fn=22; play();}
        else if(minute > 40 && minute < 50) {fn=22; play(); fn=minute-40; play();}
        else if(minute == 50) {fn=23; play();}
        else if(minute > 50 && minute < 60) {fn=23; play(); fn=minute-50; play();}
        fn=25; play();    //read "minutes"
        fn=26; play();    //read message
        OCR0A=0;    //audio off
        
   }

   minutesPB=digitalRead(minutes);
   if(minutesPB==LOW)
   {   
     minute++;
     if(minute>59) {minute=0;}
     display();
     wait();
   }

   hoursPB=digitalRead(hours);
   if(hoursPB==LOW)
   {   
     hour++;
     if(hour>23) {hour=0;} 
     display();
     wait();
   }
   
   if(TIFR1 & _BV(OCF1A))   //1 minute elapsed
     {
      OCR1A = 0xEA5F;  //59999 counts = 1 minute
      TIFR1 = _BV(OCF1A);  //TMR1 interrupt reset
 
      minute++;
      if(minute>59) {minute=0; hour++; Htalk=1;}
      if(hour>23) hour=0;
      display();

    }
}

    //find the file and play it
 void play(void)
  {
    if(!sdhc){fn += 1;}  //if not SDHC first file = 1
    if(fn > 15)
    {
      fn -=16; file(fn*32+20,1);    //32 bytes per file descriptor at offset of 20
    }
    else
    {
      file(fn*32+20,0);    //32 bytes per file descriptor at offset of 20
    }
    loc=(1 + (DataSector) + (unsigned long)(FileCluster-2) * SectorsPerCluster) * card ;
    ReadSD();
  }
  
    //LCD update
 void display(void)
  {
      lcd.setCursor(5, 0); // top left
      if(hour<10)lcd.print(" ");
      lcd.print(hour);
      lcd.print(":");
      if(minute<10)lcd.print("0");
      lcd.print(minute);
  }
  
    //SD error display
 void error(void)
  {
    lcd.setCursor(0, 1); // bottom left
    lcd.print("SD ERROR");
    lcd.setCursor(0, 0); // top left
  }
  
    //1 sec delay
  void wait(void)
  {
    unsigned long i; //,j
    for(i=0;i<100000;i++)
    {
     digitalRead(9);
    }   
  }

 void ReadSD(void)
  {
    unsigned int i,r;
    unsigned char read_data;
    digitalWrite(CS, LOW);
    r = Command(18,loc,0xFF); //read multi-sector
    if(r != 0) error();     //if command failed error();
    
    while(FileSize--)
    {
      while(spi(0xFF) != 0xFE); // wait for first byte
      for(i=0;i<512;i++){
        delayMicroseconds(26);
        OCR0A=spi(0xFF);    //write byte to PWM
        TIFR0 = _BV(OCF0A);  //TMR0 interrupt reset
    }
      spi(0xFF);  //discard of CRC
      spi(0xFF);
     }
    
    Command(12,0x00,0xFF);  //stop transmit
    spi(0xFF);
    spi(0xFF);
    digitalWrite(CS, HIGH);
    spi(0xFF);
  }

void file(unsigned int offset, unsigned char sect)  //find files
{
  unsigned int r,i;
  unsigned char fc[4], fs[4]; //
  
  digitalWrite(CS, LOW);
  r = Command(17,(RootDir+sect)*card,0xFF);   //read boot-sector for info from file entry
  if(r != 0) error();     //if command failed
  
  while(spi(0xFF) != 0xFe); // wait for first byte
  for(i=0;i<512;i++){
    if(i==offset){fc[2]=spi(0xFF);} 
    else if(i==offset+1){fc[3]=spi(0xFF);}
    else if(i==offset+6){fc[0]=spi(0xFF);}
    else if(i==offset+7){fc[1]=spi(0xFF);}
    
    else if(i==offset+8){fs[0]=spi(0xFF);}
    else if(i==offset+9){fs[1]=spi(0xFF);}
    else if(i==offset+10){fs[2]=spi(0xFF);}
    else if(i==offset+11){fs[3]=spi(0xFF);}
    else{spi(0xFF);}
    
  }
  spi(0xFF);  //discard of CRC
  spi(0xFF);
  digitalWrite(CS, HIGH);
  spi(0xFF);
  FileCluster = fc[0] | ( (unsigned long)fc[1] << 8 ) | ( (unsigned long)fc[2] << 16 ) | ( (unsigned long)fc[3] << 24 );
  FileSize = fs[0] | ( (unsigned long)fs[1] << 8 ) | ( (unsigned long)fs[2] << 16 ) | ( (unsigned long)fs[3] << 24 );
  FileSize = FileSize/512-1;    //file size in sectors
}

void fat (void)
{
  unsigned int r,i;
  unsigned char pfs[4],bps1,bps2,rs1,rs2,spf[4],rdc[4]; //pfs=partition first sector ,de1,de2,spf1,d[7]
  
  digitalWrite(CS, LOW);
  r = Command(17,0,0xFF);   //read MBR-sector
  if(r != 0) error();     //if command failed
  
  while(spi(0xFF) != 0xFe); // wait for first byte
  for(i=0;i<512;i++){
    if(i==454){pfs[0]=spi(0xFF);} //pfs=partition first sector
    else if(i==455){pfs[1]=spi(0xFF);}
    else if(i==456){pfs[2]=spi(0xFF);}
    else if(i==457){pfs[3]=spi(0xFF);}
    else{spi(0xFF);}
    
  }
  spi(0xFF);  //discard of CRC
  spi(0xFF);
  digitalWrite(CS, HIGH);
  spi(0xFF);
  //convert 4 bytes to long int
  BootSector = pfs[0] | ( (unsigned long)pfs[1] << 8 ) | ( (unsigned long)pfs[2] << 16 ) | ( (unsigned long)pfs[3] << 24 );
  
  digitalWrite(CS, LOW);
  r = Command(17,BootSector*card,0xFF);   //read boot-sector
  if(r != 0) error();     //if command failed
  
  while(spi(0xFF) != 0xFe); // wait for first byte
  for(i=0;i<512;i++){
    
    if(i==11){bps1=spi(0xFF);} //bytes per sector
    else if(i==12){bps2=spi(0xFF);}
    else if(i==13){SectorsPerCluster=spi(0xFF);}
    else if(i==14){rs1=spi(0xFF);}
    else if(i==15){rs2=spi(0xFF);}
    else if(i==16){Fats=spi(0xFF);} //number of FATs
    else if(i==36){spf[0]=spi(0xFF);}
    else if(i==37){spf[1]=spi(0xFF);}
    else if(i==38){spf[2]=spi(0xFF);}
    else if(i==39){spf[3]=spi(0xFF);}
    else if(i==44){rdc[0]=spi(0xFF);}
    else if(i==45){rdc[1]=spi(0xFF);}
    else if(i==46){rdc[2]=spi(0xFF);}
    else if(i==47){rdc[3]=spi(0xFF);}
    else{spi(0xFF);}
    
  }
  spi(0xFF);  //discard of CRC
  spi(0xFF);
  digitalWrite(CS, HIGH);
  spi(0xFF);   
  
  BytesPerSector = bps1 | ( (unsigned int)bps2 << 8 );
  ReservedSectors = rs1 | ( (unsigned int)rs2 << 8 ); //from partition start to first FAT
  RootDirCluster = rdc[0] | ( (unsigned long)rdc[1] << 8 ) | ( (unsigned long)rdc[2] << 16 ) | ( (unsigned long)rdc[3] << 24 );
  SectorsPerFat = spf[0] | ( (unsigned long)spf[1] << 8 ) | ( (unsigned long)spf[2] << 16 ) | ( (unsigned long)spf[3] << 24 );
  DataSector = BootSector + (unsigned long)Fats * (unsigned long)SectorsPerFat + (unsigned long)ReservedSectors;  // + 1  
  RootDir = (RootDirCluster -2) * (unsigned long)SectorsPerCluster + DataSector;
}  

unsigned char spi(unsigned char data)   // send character over spi
{
  SPDR = data;  // Start transmission
  while(!(SPSR & _BV(SPIF)));   // Wait for transmission to complete
  return SPDR;    // received byte

}
      //assemble a 32 bits command
char Command(unsigned char frame1, unsigned long adrs, unsigned char frame2 )
{
  unsigned char i, res;
  spi(0xFF);
  spi((frame1 | 0x40) & 0x7F);  //first 2 bits are 01
  spi((adrs & 0xFF000000) >> 24);   //first of the 4 bytes address
  spi((adrs & 0x00FF0000) >> 16);
  spi((adrs & 0x0000FF00) >> 8);
  spi(adrs & 0x000000FF);
  spi(frame2 | 1);        //CRC and last bit 1

  for(i=0;i<10;i++) // wait for received character
  {
    res = spi(0xFF);
    if(res != 0xFF)break;
  }
  return res;
}
