/*

      Zavod Meteostation v.1.7


*/

#include <SPI.h>
#include <Ethernet2.h>
#define pin_v A1
#define Pin_sign 13
#include "DHT.h"
#define DHTPIN 9
#define PinWet 3
#define PinRain 5
#define PinRad 7
#define PinWetN A2
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
#include <SFE_BMP180.h>
#include <Wire.h>
#include <RTCZero.h>
#include <BH1750.h>

struct AllData{
  int hh,mm,ss,dow,dd,mon,yyyy; // время и дата
  float Volt;
  int Bat_lvl;
  float hum,temp;       // влажность температура от DHT22
  double p,p0;        // давление и температура ще BMP180
  double TV;
  uint16_t lux; // осв в люксах BH1750
  double alt;    //высота над ур моря
  double longi , lati; // долгота и широта
  int nos_napr;
  float nos_sil;  // напр и сила ветра
  float os_mm; // осадки в мм
  float radio; // уровень радиации в µSv/h
  };

byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress my_IP(192, 168, 0, 172); // our statiC IP
IPAddress my_Dns(192, 168, 0, 1);
IPAddress my_gate(192, 168, 0, 1);
IPAddress my_subnet(255, 255, 255, 0);
/*IPAddress my_IP(10, 10, 5, 2); // our statiC IP
IPAddress my_Dns(8,8,8,8);
IPAddress my_gate(10,10,5,1);
IPAddress my_subnet(255,255,255,252);*/
//byte serv_IP[] = {192, 168, 0, 170}; // server IP
char serv_IP[]="nasodessahome.myqnapcloud.com";
int serv_port=8070;
int voskr, intrv = 20, shift = 2, prev_ss, next_ss;

unsigned long scht_w, scht_r, scht_rad;
int  input_n;
int min_in [] = {10,51, 96, 300, 800, 620, 450, 191},
    max_in [] = {50, 95, 190, 420, 900, 799, 610, 299};
int degr;
float x_w,y_w, sw;

EthernetClient client;

RTCZero rtc;  
DHT dht(DHTPIN, DHTTYPE);
SFE_BMP180 pressure;
BH1750 lightMeter;

AllData zsd;
int input_v;
char statusPT;
String send_data, ans;
int cur_hh,cur_mm, cur_ss,cur_dow,cur_dd, cur_mon, cur_yyyy;

//счетчик сил ветр и дожд
void weter(){
  scht_w++;
}

void rain(){
  scht_r++;
}

void rad(){
  scht_rad++;
}


// конверсия double (float) в строку
String DtoS(double a, byte dig) {
  double b,c;
  double* pc =&c;
  String s="";
  long bef;                      
  int i,digit;
  b=modf(a,pc);
  bef = c;
  s+=String(bef)+String(".");
  if (b<0) {b=-b;};
  for (i = 0; i < dig; i++) {
         b *= 10.0; 
         digit = (int) b;
         b = b - (float) digit;
     s+= String(digit);
     }
  return s;  
}

// преобразование структуры всех данных в строку
String con_DtoS(AllData d){
  String buf; 
  String s = "{";
  s +=String (d.hh); s+=";";
  s +=String (d.mm); s+=";";
  s +=String (d.ss); s+=";";
  s +=String (d.dow); s+=";";
  s +=String (d.dd); s+=";";
  s +=String (d.mon); s+=";";
  s +=String (d.yyyy); s+=";";
  buf = DtoS(d.Volt, 2); s += buf; s+=";";
  s +=String (d.Bat_lvl); s+=";";
  buf = DtoS(d.hum,3); s += buf; s+=";";
  buf = DtoS(d.temp,3); s += buf; s+=";";
  buf = DtoS(d.p,3); s += buf; s+=";";
  buf = DtoS(d.p0,3); s += buf; s+=";";
  buf = DtoS(d.TV,3); s += buf; s+=";";
  s +=String (d.lux); s+=";";
  buf = DtoS(d.alt,2); s += buf; s+=";";
  buf = DtoS(d.longi,7); s += buf; s+=";";
  buf = DtoS(d.lati,7); s += buf; s+=";";
  s +=String (d.nos_napr); s+=";"; 
  buf = DtoS(d.nos_sil,3); s += buf; s+=";"; 
  buf = DtoS(d.os_mm,2); s += buf; s+=";"; 
  buf = DtoS(d.radio,3); s += buf; s+=";"; 
  s +="}"; 
  
  return s;
}

// день недели
/*int dowFunc (int year, int month, int day)
{
  int leap = year*365 + (year/4) - (year/100) + (year/400);
  year += ((month+9)/12) - 1;
  month = (month+9) % 12;
  int zeller = leap  + month*30 + ((6*month+5)/10) + day + 1;
  return (zeller % 7);
  }*/

int dowFunc (int _year, int _month, int _day)
{
  int _week;
  if (_month == 1 || _month == 2) {
    _month += 12;
    --_year;
  }
  _week = (_day + 2 * _month + 3 * (_month + 1) / 5 + _year + _year / 4 - _year / 100 + _year / 400) % 7;
//  ++_week;
  return _week;
} 

  // сет интернал тиме from www
  void TimeSET_www()
  {
  for(int j=1; j<=10; j++){
  for(int i=1; i<=10; i++){
      client.connect("api.timezonedb.com", 80);
      if(client.connected()){
        break;
      }
   }
  client.println("GET /v2/get-time-zone?key=4JZ68AWE4A9D&format=json&by=zone&zone=Europe/Kiev HTTP/1.1");
  client.println("Host: api.timezonedb.com");
  client.println("Connection: close");
  client.println(); 
  long _startMillis = millis();
  while (!client.available() and (millis()-_startMillis < 2000))
  {
  };
  ans="";
  if (client.available()) {
       int av_char=client.available();
       char c;
       for (int k=0; k<av_char; k++) {
        c=client.read();  // Serial.println(c); 
        ans+=String(c);
       }
  }
  client.stop();
  Serial.println(ans);
  int tpos=ans.indexOf("timestamp");
  if( tpos!=-1){
      tpos+=11;
      int tpose=ans.indexOf(",", tpos);
      String time_www= ans.substring(tpos, tpose);
      long e_time_www=time_www.toInt();
      rtc.setEpoch( e_time_www );  
      break;
  }
  }
  }



void setup()
{
  pinMode(Pin_sign,OUTPUT);
  digitalWrite(Pin_sign,LOW);  
  
  // initialize serial for debugging
  Serial.begin(115200);

  Ethernet.begin(mac, my_IP, my_Dns, my_gate, my_subnet);

  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());
  
  rtc.begin();
  Serial.println("rtc");
  dht.begin();
  Serial.println("dht");
  pressure.begin();
  Serial.println("pres");
  lightMeter.begin();
  Serial.println("light");
  
   TimeSET_www();

  zsd.alt = 100.0;     // актуальные данные по заводу
  zsd.lati=46.6213889;
  zsd.longi=30.8063889;
  zsd.nos_napr=0;
  zsd.nos_sil=0.0;
  zsd.os_mm=0.0;
  zsd.Volt=0;
  zsd.Bat_lvl=0;
  zsd.radio=0.0;

  scht_r=0; scht_w=0; scht_rad=0;
  x_w=0.0; y_w=0.0;
  prev_ss=rtc.getSeconds();  next_ss=(prev_ss/10+1)*10; if (next_ss==60) next_ss=0;
  
pinMode(PinWet, INPUT_PULLUP);
attachInterrupt(PinWet, weter, RISING);
pinMode(PinRain, INPUT_PULLUP);
attachInterrupt(PinRain, rain, RISING);
pinMode(PinRad, INPUT);
attachInterrupt(PinRad, rad, RISING);

Serial.println(rtc.getHours());
Serial.println(rtc.getMinutes());
Serial.println(rtc.getSeconds());
Serial.println(rtc.getDay());
Serial.println(rtc.getMonth());
Serial.println(rtc.getYear()+2000); 
 
/*  
  voskr=(dt.minute/intrv)*intrv+shift;*/
  
}


void loop()
{  
  // время и дата cur_hh,cur_mm, cur_ss,cur_dow,cur_dd, cur_mon, cur_yyyy;
  cur_hh=rtc.getHours();
  cur_mm=rtc.getMinutes();
  cur_ss=rtc.getSeconds();
  cur_dd=rtc.getDay();
  cur_mon=rtc.getMonth();
  cur_yyyy=rtc.getYear()+2000; 

  if (cur_hh==0 && cur_mm==5 && cur_ss==10) {
    TimeSET_www();
Serial.println(rtc.getHours());
Serial.println(rtc.getMinutes());
Serial.println(rtc.getSeconds());
Serial.println(rtc.getDay());
Serial.println(rtc.getMonth());
Serial.println(rtc.getYear()+2000); 
   }

  // замеры сил и напр ветра 10 сек
  if(cur_ss==next_ss){
    prev_ss=next_ss-prev_ss; if (prev_ss<0) prev_ss+=60;
    sw= (scht_w*0.6667)/prev_ss;
    scht_w=0; 
    input_n = analogRead(PinWetN);
    for(int i=0; i<=7; i++){
       if((input_n<max_in[i])&&(input_n>min_in[i])){
       degr=i*45;
       break;
    }
    }
    x_w=x_w+sw*cos(degr* PI / 180);
    y_w=y_w+sw*cos((degr-90)* PI / 180);
    prev_ss=next_ss; next_ss+=10; if (next_ss==60) next_ss=0;
  }

  if ((cur_mm%10==3) && (cur_ss==10)){
// if ((cur_mm%2==1) && (cur_ss==10)){
  digitalWrite(Pin_sign,HIGH);  

  // время и дата 
  zsd.hh=rtc.getHours();
  zsd.mm=rtc.getMinutes();
  zsd.ss=rtc.getSeconds();
  zsd.dd=rtc.getDay();
  zsd.mon=rtc.getMonth();
  zsd.yyyy=rtc.getYear()+2000;
  zsd.dow=dowFunc(zsd.yyyy,zsd.mon,zsd.dd);;
    
  // измерение напряжения батареи
  input_v = analogRead(pin_v);
  zsd.Volt = ((float)input_v/1023)*6.6;
  zsd.Bat_lvl = ((zsd.Volt - 2.4)/1.8)*100; 

  // измерение влажности и температуры
  delay (1000);
  do {
  zsd.hum = dht.readHumidity();
  zsd.temp = dht.readTemperature();
  } while (isnan(zsd.hum));
  //  Serial.println(  zsd.hum); 
  
 //проверка давления 
  statusPT = pressure.startTemperature();
  if (statusPT != 0)
  {
    // Wait for the measurement to complete:
    delay(statusPT); 
  statusPT = pressure.getTemperature(zsd.TV); }
  if (statusPT != 0)
    {
      statusPT = pressure.startPressure(3);
      if (statusPT != 0)
      {
        // Wait for the measurement to complete:
        delay(statusPT);
       statusPT = pressure.getPressure(zsd.p,zsd.TV);
       zsd.p0 = pressure.sealevel(zsd.p,zsd.alt); 
       zsd.p= zsd.p*0.7500637554;
       zsd.p0= zsd.p0*0.7500637554;
      }
    } 

  // проверка осв
  zsd.lux = lightMeter.readLightLevel();

  // перевод дождя в дождь
  zsd.os_mm=scht_r*0.2794;
  scht_r=0;

  // перевод перевод тика в зиверты
  zsd.radio=(scht_rad/10)*0.008120;
  scht_rad=0;

  // перевод ветра в ветер
  zsd.nos_sil=sqrt(sq(x_w)+sq(y_w))/60.0;
  if (zsd.nos_sil==0.0) { zsd.nos_napr=0; }
  else { zsd.nos_napr=(acos((x_w/60.0)/zsd.nos_sil)/PI)*180; }
  x_w=0.0; y_w=0.0; 

  send_data=con_DtoS(zsd);
  send_data=String("POSTMZ")+send_data+String("\r\n\r\n");
  Serial.println(send_data);    

   for (int i=0; i<10; i++) {
   client.stop();
   for (int j=0; j<20; j++) {
    if (client.connect(serv_IP, serv_port)) {
      Serial.println("Connected"); 
    // send all data 
      client.print(send_data);
      Serial.println("Sent"); 
      break; }
    }
     // and got answer
     for (int j=0; j<20; j++) {
      delay(100);
      ans="";
      if (client.available()) {
       int av_char=client.available();
       char c;
       for (int k=0; k<av_char; k++) {
        c=client.read();  // Serial.println(c); 
        ans+=String(c);
       }
       break;   
      }
     }
    Serial.println(ans);    
//    if (!(ans==String(""))) { break; }
    if (ans.indexOf("654321")!=-1) { break; }
   }
  client.flush();  
  client.stop(); 

//    сброс счетчика вертушки
      scht_w=0;
      prev_ss=rtc.getSeconds();  next_ss=(prev_ss/10+1)*10; if (next_ss==60) next_ss=0;      
      digitalWrite(Pin_sign,LOW);  
  }
 
 }


