//this is firmware for the revised custom TB scope board

#include <SPI.h>
#include <Wire.h> //required for temp/humidity sensor
#include <boards.h>
#include <RBL_nRF8001.h>

#define MICROSTEP1 34
#define MICROSTEP2 35
#define MICROSTEP3 36
#define X_STEP_PIN    22
#define X_DIR_PIN    23
#define X_EN_PIN    24
#define Y_STEP_PIN    25
#define Y_DIR_PIN    26
#define Y_EN_PIN    27
#define Z_STEP_PIN    28
#define Z_DIR_PIN    29
#define Z_EN_PIN    30
#define FL_LED_PWM_PIN 44
#define FL_LED_EN_PIN 47
#define BF_LED_PWM_PIN 11
#define BF_LED_EN_PIN 37
#define X_LIMIT_PIN A8
#define Y_LIMIT_PIN A9
#define Z_LIMIT_PIN A10

#define GREEN_INDICATOR_PIN 40
#define RED_INDICATOR_PIN 41

#define KILL_PIN 43
#define BATT_PIN A14

#define X_HOME_DIR 1
#define Y_HOME_DIR 0
#define Z_HOME_DIR 0

#define POSITION_LIMIT 0
#define POSITION_TEST_TARGET 1
#define POSITION_SLIDE_CENTER 2
#define POSITION_LOADING 3

#define DEFAULT_HALFSTEP_INTERVAL 200 //this is the speed used for homing, moving to slide center, loading, etc.

//all coordinates are relative to limit switches
#define TEST_TARGET_X 3310
#define TEST_TARGET_Y 3020
#define TEST_TARGET_Z 15900
#define SLIDE_CENTER_X 800
#define SLIDE_CENTER_Y 4000
#define SLIDE_CENTER_Z 21900
#define LOADING_X 2000
#define LOADING_Y 10000
#define LOADING_Z 0

#define BLE_BLINK_FREQ 1
#define TIMEOUT_DURATION 20000

//top 3 bits = opcode
#define CMD_MOVE 1
#define CMD_SET_SPEED 2
#define CMD_SPECIAL_POSITION 3
#define CMD_SET_LIGHT 4
#define CMD_GET_STATE 5

#define LIMIT_SWITCH_THRESHOLD 10

//Servo myservo;

static char device_name[11] = "TB Scope";

unsigned int half_step_interval = 500; //micros

unsigned int move_stage(byte axis, byte dir, unsigned int half_step_interval, boolean stop_on_home, boolean disable_after, unsigned int num_steps)
{
  Serial.print(axis,DEC); Serial.print(' '); Serial.print(dir,DEC); Serial.print(' '); Serial.print(half_step_interval,DEC); Serial.print(' '); Serial.print(stop_on_home,DEC); Serial.print(' '); Serial.print(disable_after,DEC); Serial.print(' '); Serial.println(num_steps,DEC);
  
  byte step_pin; byte dir_pin; byte en_pin; byte limit_switch_pin; byte home_dir;

  //get pins for this axis
  switch (axis) {
    case 0x1: //x
      step_pin = X_STEP_PIN;
      dir_pin = X_DIR_PIN;
      en_pin = X_EN_PIN;
      limit_switch_pin = X_LIMIT_PIN;
      home_dir = X_HOME_DIR;
      break;
    case 0x2: //y
      step_pin = Y_STEP_PIN;
      dir_pin = Y_DIR_PIN;
      en_pin = Y_EN_PIN;
      limit_switch_pin = Y_LIMIT_PIN;
      home_dir = Y_HOME_DIR;      
      break;
    case 0x3: //z
      step_pin = Z_STEP_PIN;
      dir_pin = Z_DIR_PIN;
      en_pin = Z_EN_PIN;
      limit_switch_pin = Z_LIMIT_PIN;
      home_dir = Z_HOME_DIR;      
      break;
  }

  //set direction
  if (dir==0) 
    digitalWrite(dir_pin,LOW);
  else if (dir==1)
    digitalWrite(dir_pin,HIGH);
  
  //turn on motor
  if (num_steps>0)
    digitalWrite(en_pin,LOW);
  
  //do stepping
  unsigned int i;
  for (i=0;i<num_steps;i++)
  {
    if (stop_on_home && dir==home_dir)
      if (analogRead(limit_switch_pin)<LIMIT_SWITCH_THRESHOLD)
        break;
        
    //step    
    digitalWrite(step_pin, HIGH);
    delayMicroseconds(half_step_interval);
    digitalWrite(step_pin, LOW);
    delayMicroseconds(half_step_interval);
   }  

   if (disable_after)
     digitalWrite(en_pin,HIGH);

   //send response
   notify();
   
   return i;
}

void enable_motor(byte axis, boolean enabled) {
  byte en_pin;
  switch (axis) {
    case 0x1: //x
      en_pin = X_EN_PIN;
      break;
    case 0x2: //y
      en_pin = Y_EN_PIN;
      break;
    case 0x3: //z
      en_pin = Z_EN_PIN;
      break;
  } 
  
  if (enabled)
    digitalWrite(en_pin,LOW);
  else
    digitalWrite(en_pin,HIGH);
}

void goto_special_position(byte position) {

  switch (position) {
    case POSITION_LIMIT:
    {
      //move_stage(3,1,100,1,1,200); //backup in Z
      //while (move_stage(3,0,100,1,1,20000)==20000); 
      
      unsigned long timeout_millis = millis() + TIMEOUT_DURATION;
      move_stage(2,1,DEFAULT_HALFSTEP_INTERVAL,1,1,200); //backup in Y
      while ((move_stage(2,0,DEFAULT_HALFSTEP_INTERVAL,1,1,10000)==10000)) //move Y, and continue until it hits limit (if the tray is out/unmeshed to gear, it will keep spinning)
      {
        if (millis()>timeout_millis)
          return;
      }
      
      move_stage(1,0,DEFAULT_HALFSTEP_INTERVAL,1,1,200); //backup in X
      
      while ((move_stage(1,1,DEFAULT_HALFSTEP_INTERVAL,1,1,10000)==10000)) //move X till limit
      {
        if (millis()>timeout_millis)
          return;
      }
      
      break;    
    }
    case POSITION_TEST_TARGET:
      goto_special_position(POSITION_LIMIT);
      move_stage(2,1,DEFAULT_HALFSTEP_INTERVAL,1,1,TEST_TARGET_Y);
      move_stage(1,0,DEFAULT_HALFSTEP_INTERVAL,1,1,TEST_TARGET_X);
      //move_stage(3,1,100,1,1,TEST_TARGET_Z);
      break;
      
    case POSITION_SLIDE_CENTER:
      goto_special_position(POSITION_LIMIT);
      move_stage(2,1,DEFAULT_HALFSTEP_INTERVAL,1,1,SLIDE_CENTER_Y);
      move_stage(1,0,DEFAULT_HALFSTEP_INTERVAL,1,1,SLIDE_CENTER_X);  
      //move_stage(3,1,100,1,1,SLIDE_CENTER_Z);  
      break;
    case POSITION_LOADING:
      goto_special_position(POSITION_LIMIT);
      move_stage(1,0,DEFAULT_HALFSTEP_INTERVAL,1,1,LOADING_X);        
      move_stage(2,1,DEFAULT_HALFSTEP_INTERVAL,1,1,LOADING_Y);
      break;
  }  
  
  notify();
}

void notify()
{
  byte buf[3] = {0xFF, 0x00, 0x00}; //0xFF = limit switch status
  if (analogRead(X_LIMIT_PIN)<LIMIT_SWITCH_THRESHOLD)
    buf[1] |= 0b00000100;
  if (analogRead(Y_LIMIT_PIN)<LIMIT_SWITCH_THRESHOLD)
    buf[1] |= 0b00000010;    
  if (analogRead(Z_LIMIT_PIN)<LIMIT_SWITCH_THRESHOLD)
    buf[1] |= 0b00000001;        
    
  ble_write_bytes(buf,3);
}

//sends battery, temp, and humidity status
void send_status()
{
  byte buf[3] = {0x00, 0x00, 0x00};
  
  unsigned int batt_voltage;
  batt_voltage = analogRead(BATT_PIN);
  buf[0] = 0xFE; //0xFE = battery
  buf[1] = (batt_voltage & 0xFF00)>>8; //high byte
  buf[2] = (batt_voltage & 0x00FF); //low byte
  ble_write_bytes(buf,3);
  delay(50);
  
  unsigned int humidity;
  unsigned int temperature;
  fetch_humidity_temperature(&humidity,&temperature);
  buf[0] = 0xFD; //0xFD = temperature
  buf[1] = (temperature & 0xFF00)>>8; //high byte
  buf[2] = (temperature & 0x00FF); //low byte
  ble_write_bytes(buf,3);
  delay(50);
  
  buf[0] = 0xFC; //0xFC = humidity
  buf[1] = (humidity & 0xFF00)>>8; //high byte
  buf[2] = (humidity & 0x00FF); //low byte  
  ble_write_bytes(buf,3);
  delay(50);
  
}

void setup()
{
  Wire.begin();        // join i2c bus for temp/humidity sensor

  Serial.begin(57600);
  Serial.println("TB Scope Firmware 2015-07-29");
   
  pinMode(MICROSTEP1, OUTPUT);
  pinMode(MICROSTEP2, OUTPUT);
  pinMode(MICROSTEP3, OUTPUT);   
  pinMode(X_STEP_PIN, OUTPUT);
  pinMode(X_DIR_PIN, OUTPUT);
  pinMode(X_EN_PIN, OUTPUT);
  pinMode(Y_STEP_PIN, OUTPUT);
  pinMode(Y_DIR_PIN, OUTPUT);
  pinMode(Y_EN_PIN, OUTPUT);
  pinMode(Z_STEP_PIN, OUTPUT);
  pinMode(Z_DIR_PIN, OUTPUT);
  pinMode(Z_EN_PIN, OUTPUT);
  pinMode(X_LIMIT_PIN, INPUT);
  pinMode(Y_LIMIT_PIN, INPUT);
  pinMode(Z_LIMIT_PIN, INPUT);
  
  pinMode(RED_INDICATOR_PIN, OUTPUT);
  pinMode(GREEN_INDICATOR_PIN, OUTPUT);
  
  pinMode(FL_LED_EN_PIN, OUTPUT);
  pinMode(BF_LED_EN_PIN, OUTPUT);
  
  pinMode(KILL_PIN, OUTPUT);
 
  //set microstepping to 1/16 step
  digitalWrite(MICROSTEP1, HIGH); 
  digitalWrite(MICROSTEP2, HIGH);   
  digitalWrite(MICROSTEP3, HIGH); 
  
  digitalWrite(X_STEP_PIN, HIGH); 
  digitalWrite(Y_STEP_PIN, HIGH);   
  digitalWrite(Z_STEP_PIN, HIGH);
  digitalWrite(X_DIR_PIN, HIGH); 
  digitalWrite(Y_DIR_PIN, HIGH); 
  digitalWrite(Z_DIR_PIN, HIGH); 
  digitalWrite(X_EN_PIN, HIGH);   
  digitalWrite(Y_EN_PIN, HIGH);   
  digitalWrite(Z_EN_PIN, HIGH);   
  
  digitalWrite(RED_INDICATOR_PIN, LOW);
  digitalWrite(GREEN_INDICATOR_PIN, HIGH);
  
  digitalWrite(FL_LED_EN_PIN, LOW);
  analogWrite(FL_LED_PWM_PIN, 0x00); 
  
  digitalWrite(BF_LED_EN_PIN, HIGH);   
  analogWrite(FL_LED_PWM_PIN, 0x00);
  
  digitalWrite(KILL_PIN, LOW);
  
  Serial.println("start");
    
  ble_begin();

  ble_set_name(device_name);

  //goto_special_position(POSITION_LOADING);
}

byte fetch_humidity_temperature(unsigned int *p_H_dat, unsigned int *p_T_dat)
{
      byte address, Hum_H, Hum_L, Temp_H, Temp_L, _status;
      unsigned int H_dat, T_dat;
      address = 0x27;
      Wire.beginTransmission(address); 
      Wire.endTransmission();
      delay(100);
      
      Wire.requestFrom((int)address, (int) 4);
      Hum_H = Wire.read();
      Hum_L = Wire.read();
      Temp_H = Wire.read();
      Temp_L = Wire.read();
      Wire.endTransmission();
      
      _status = (Hum_H >> 6) & 0x03;
      Hum_H = Hum_H & 0x3f;
      H_dat = (((unsigned int)Hum_H) << 8) | Hum_L;
      T_dat = (((unsigned int)Temp_H) << 8) | Temp_L;
      T_dat = T_dat / 4;
      *p_H_dat = H_dat;
      *p_T_dat = T_dat;
      return(_status);
}

void loop()
{
  static boolean analog_enabled = false;
  static byte old_state = LOW;
  
  // If data is ready
  while(ble_available())
  {
    // read out command and data
    byte data0 = ble_read(); //command
    byte data1 = ble_read(); //arg 1
    byte data2 = ble_read(); //arg 2
    
    byte cmd = (data0 & B11100000) >> 5;
    
    Serial.print(data0,HEX);
    Serial.print(' ');
    Serial.print(data1,HEX);
    Serial.print(' ');
    Serial.println(data2,HEX);
    
    byte axis;
    byte dir;
    boolean stop_on_home;
    boolean disable_after;
    unsigned int num_steps;
    
    switch (cmd) {
      case CMD_MOVE:
        axis = (data0 & B00011000) >> 3;
        dir =  (data0 & B00000100) >> 2;
        stop_on_home = (data0 & B00000010) >> 1;
        disable_after = (data0 & B00000001);
        num_steps = (data1 << 8) | data2;
        
        move_stage(axis,dir,half_step_interval,stop_on_home,disable_after,num_steps);
        notify();
        
        break;
      case CMD_SET_SPEED:
        half_step_interval = (data1 << 8) | data2;
        
        break;
        
      case CMD_SPECIAL_POSITION:
        goto_special_position(data1);
        break;
        
      case CMD_SET_LIGHT:
        if (data1==0x01) {
          analogWrite(FL_LED_PWM_PIN,data2/10); //temp scaling factor
          if (data2>0)
            digitalWrite(FL_LED_EN_PIN, HIGH);
          else
            digitalWrite(FL_LED_EN_PIN, LOW);
        }
        else if (data1==0x02) { 
          analogWrite(BF_LED_PWM_PIN,0xFF-data2);
          if (data2>0)
            digitalWrite(BF_LED_EN_PIN, LOW);
          else
            digitalWrite(BF_LED_EN_PIN, HIGH);
        }
        break;
        
      case CMD_GET_STATE:
        send_status();
        break;
    }
  }  
    
  if (ble_connected())
  {
    digitalWrite(GREEN_INDICATOR_PIN,LOW);
    digitalWrite(RED_INDICATOR_PIN,HIGH);
    
  }
  else
  {
    
    if (sin(2*3.14159*BLE_BLINK_FREQ*millis()/1000.0)>0)
     digitalWrite(RED_INDICATOR_PIN,HIGH);
    else
     digitalWrite(RED_INDICATOR_PIN,LOW);
     
    digitalWrite(GREEN_INDICATOR_PIN,HIGH);
  }
  
  // Allow BLE Shield to send/receive data
  ble_do_events();  

}


