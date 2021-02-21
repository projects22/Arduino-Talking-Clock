/*
 * oled DFPlayer talking clock
 *
 * Created: 10/02/2021
 *  Author: moty22.co.uk
 */ 
#include <Wire.h>

// standard ascii 5x7 font 
static const unsigned char  font[] = {
  0x3E, 0x41, 0x41, 0x41, 0x3E,   //un-crossed 0
  0x00, 0x42, 0x7F, 0x40, 0x00,
  0x72, 0x49, 0x49, 0x49, 0x46, 
  0x21, 0x41, 0x49, 0x4D, 0x33, 
  0x18, 0x14, 0x12, 0x7F, 0x10, 
  0x27, 0x45, 0x45, 0x45, 0x39, 
  0x3C, 0x4A, 0x49, 0x49, 0x31, 
  0x41, 0x21, 0x11, 0x09, 0x07, 
  0x36, 0x49, 0x49, 0x49, 0x36,
  0x46, 0x49, 0x49, 0x29, 0x1E,   //9 
  0x00, 0x00, 0x14, 0x00, 0x00   //: 
};

  unsigned char addr=0x3C, minute=34, hour=12, hourly=0;  //0x78
  unsigned char vh[]={0x7E,0xff,0x06,0x04,0x00,0x00,0x00,0xEF};  //volume up command
  unsigned char vl[]={0x7E,0xff,0x06,0x05,0x00,0x00,0x00,0xEF}; 
   
void setup() {

      //1000 Hz - timer2
    OCR2A =249;
    OCR2B = 125;  
    TCCR2A = _BV(COM2B1) | _BV(COM2B0) | _BV(WGM21) | _BV(WGM20);  //output B in phase, fast PWM mode
    TCCR2B = _BV(WGM22) | _BV(CS22); // set prescaler To 64 And start the timer
    pinMode(3, OUTPUT);
    
        //60 sec - timer1
    TCCR1A = _BV(WGM10) | _BV(WGM11) | _BV(COM1A0); //   
    TCCR1B = _BV(WGM12) | _BV(WGM13) | _BV(CS11) | _BV(CS12);  // input T1, PWM mode

    pinMode(6, INPUT_PULLUP);  //minutes PB
    pinMode(7, INPUT_PULLUP);  //hour PB
    pinMode(8, INPUT_PULLUP);  //talk PB
    pinMode(9, INPUT_PULLUP);  //volume +
    pinMode(10, INPUT_PULLUP);  //volume -
    pinMode(2, INPUT);        //busy
    Serial.begin(9600);
    Wire.begin();        // init i2c bus
    Wire.setClock(200000);  //200khz
    oled_init();
    clrScreen();
    oled();
}

void loop()
{

   if(TIFR1 & _BV(OCF1A))   //1 minute time
   {
      OCR1A = 0xEA5F;  //59999 counts = 1 minute
      TIFR1 = _BV(OCF1A);  //TMR1 interrupt reset
      minute++;
      if(minute>59) {minute=0; hour++; hourly=1;}
      if(hour>23){ hour=0;}
      oled();
      if(hourly){hourly=0; play_df();}  //comment line to disable hourly talk
   }

   if(!digitalRead(6)) //set hours
   {
      minute=minute+1;
      if (minute>59) minute=0;
      oled();
      wait(500);
   }
   if(!digitalRead(7)) //set minutes
   {
      hour=hour+1;
      if (hour>23) hour=0;
      oled();
      wait(500);
   }
   if(!digitalRead(8)){play_df(); wait(500);}  //talk
   if(!digitalRead(9)){Serial.write(vh,8); wait(500);} //vol+
   if(!digitalRead(10)){Serial.write(vl,8); wait(500);}  //vol-

}

void wait(unsigned int ms){ //delay
  unsigned int i,j;
  for(i=0;i<ms;i++){
    for(j=0;j<200;j++){
     byte b=digitalRead(4);
    }
  }
}


void play_df(){
    //select track 
  if(hour>20)
  {
    cmnd(21);
    cmnd(hour - 19);
  }else{
    cmnd(hour + 1);
  }
  cmnd(25);  //hour

  if (minute<21){cmnd(minute + 1);}
  else if(minute>20 && minute<30){cmnd(21); cmnd(minute - 19);}
  else if(minute==30){cmnd(22);}    //30
  else if (minute>30 && minute<40){cmnd(22); cmnd(minute - 29);}
  else if (minute==40){cmnd(23);} //40
  else if (minute>40 && minute<50){cmnd(23); cmnd(minute - 39);}
  else if (minute==50){cmnd(24);} //50
  else if (minute>50 && minute<60){cmnd(24); cmnd(minute - 49);}

  cmnd(26);  //minutes
  cmnd(27);  //message 
}

void oled()
{
  drawChar2(minute % 10,1,5);
  drawChar2(minute/10,1,4);
  drawChar2(10,1,3);
  drawChar2(hour % 10,1,2);
  drawChar2(hour/10,1,1);
}

void cmnd(unsigned char track)
{
  unsigned char cb[]={0x7E,0xff,0x06,0x03,0x00,0x00,track,0xEF};

  while(!digitalRead(2)){  }
  Serial.write(cb,8);
  wait(100);
}

void clrScreen()    //fill screen with 0
{
    unsigned char y, x;

    for ( y = 0; y < 8; y++ ) {
      for (x = 0; x < 17; x++){
          command(0x21);     //col addr
          command(8 * x); //col start
          command(8 * x + 7);  //col end
          command(0x22);    //0x22
          command(y); // Page start
          command(y); // Page end
          
          Wire.beginTransmission(addr);
          Wire.write(0x40);
          for (unsigned char i = 0; i < 8; i++){
               Wire.write(0x00);          
          }
          Wire.endTransmission();         
      }
     
    }
}

void command( unsigned char comm){
    Wire.beginTransmission(addr); 
    Wire.write(0x00);    
    Wire.write(comm); // LSB
    Wire.endTransmission();       
}

void oled_init() {
    
    command(0xAE);   // DISPLAYOFF
    command(0x8D);         // CHARGEPUMP *
    command(0x14);     //0x14-pump on
    command(0x20);         // MEMORYMODE
    command(0x0);      //0x0=horizontal, 0x01=vertical, 0x02=page
    command(0xA1);        //SEGREMAP * A0/A1=top/bottom 
    command(0xC8);     //COMSCANDEC * C0/C8=left/right
    command(0xDA);         // SETCOMPINS *
    command(0x22);   //0x22=4rows, 0x12=8rows
    command(0x81);        // SETCONTRAST
    command(0xFF);     //0x8F
    //following settings are set by default
//    command(0xD5);  //SETDISPLAYCLOCKDIV 
//    command(0x80);  
//    command(0xA8);       // SETMULTIPLEX
//    command(0x3F);     //0x1F
//    command(0xD3);   // SETDISPLAYOFFSET
//    command(0x0);  
//    command(0x40); // SETSTARTLINE  
//    command(0xD9);       // SETPRECHARGE
//    command(0xF1);
//    command(0xDB);      // SETVCOMDETECT
//    command(0x40);
//    command(0xA4);     // DISPLAYALLON_RESUME
//    command(0xA6);      // NORMALDISPLAY
    command(0xAF);          //DISPLAYON

}

    //size x1 chars
void drawChar(char fig, unsigned char y, unsigned char x)
{
    
    command(0x21);     //col addr
    command(7 * x); //col start
    command(7 * x + 4);  //col end
    command(0x22);    //0x22
    command(y); // Page start
    command(y); // Page end
    
    Wire.beginTransmission(addr);
    Wire.write(0x40);
    for (unsigned char i = 0; i < 5; i++){
         Wire.write(font[5*(fig-32)+i]);          
    }
    Wire.endTransmission();
 
 }
    //size x2 chars
void drawChar2(char fig, unsigned char y, unsigned char x)
{
    unsigned char i, line, btm, top;    //
    
    command(0x20);    // vert mode
    command(0x01);

    command(0x21);     //col addr
    command(19 * x); //col start
    command(19 * x + 14);  //col end
    command(0x22);    //0x22
    command(y); // Page start
    command(y+1); // Page end
 
    Wire.beginTransmission(addr);
    Wire.write(0x40);

    for (i = 0; i < 5; i++){
        line=font[5*fig+i];
       //line=font[5*(fig-32)+i];
        btm=0; top=0;
            // expend char    
        if(line & 64) {btm +=192;}
        if(line & 32) {btm +=48;}
        if(line & 16) {btm +=12;}           
        if(line & 8) {btm +=3;}
        
        if(line & 4) {top +=192;}
        if(line & 2) {top +=48;}
        if(line & 1) {top +=12;} 
               
         Wire.write(top); //top page
         Wire.write(btm);  //second page
         Wire.write(top);
         Wire.write(btm);
         Wire.write(top);
         Wire.write(btm);
    }
    Wire.endTransmission();
        
    command(0x20);      // horizontal mode
    command(0x00);    
        
}
