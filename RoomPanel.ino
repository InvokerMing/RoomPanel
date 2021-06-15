#include <U8x8lib.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <Wire.h>
#include <dht11.h>
dht11 DHT11;
#define DHT11PIN 13

//wifi
#include <SoftwareSerial.h>
SoftwareSerial mySerial(11, 12); // RX, TX
#define SENDOK "SEND OK"

//时钟
ThreeWire myWire(8, 10, 9); // DAT, CLK, RST
RtcDS1302<ThreeWire> Rtc(myWire);
U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);
//U8X8_SSDledPin27_SEEED_96X96_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE); //已改库
#define countof(a) (sizeof(a) / sizeof(a[0]))

//红外
#define TIMER_DISABLE_INTR  (TIMSK2 = 0)
#define SYSCLOCK  16000000
#define TIMER_CONFIG_KHZ(val) ({ \
    const uint8_t pwmval = SYSCLOCK / 2000 / (val); \
    TCCR2A               = _BV(WGM20); \
    TCCR2B               = _BV(WGM22) | _BV(CS20); \
    OCR2A                = pwmval; \
    OCR2B                = pwmval / 3; \
  })
#define TIMER_ENABLE_PWM    (TCCR2A |= _BV(COM2B1))
#define TIMER_DISABLE_PWM   (TCCR2A &= ~(_BV(COM2B1)))

//按钮
#include <Keypad.h>
const byte ROWS = 1;
const byte COLS = 4;
char hexaKeys[ROWS][COLS] = {
  {'0', '1', '2', '3'},
};
byte rowPins[ROWS] = {A1}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {5, 7, A3, 2}; //connect to the column pinouts of the keypad
Keypad customKeypad = Keypad( makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

int IRPin = 3;
int rotaryPin = A0;
int rotaryValue = 0;
bool state_LIGHT = false;
const int ledPin = 4;
RtcDateTime t;
bool state_AC = false;
bool state_FAN = false;
int temp_AC = 26;
int mode_AC = 1;
int speed_AC = 0;
long ct = 0;
int choose = 0;
bool SC_AC = false;
bool SO_AC = false;
int SC_Hour = 12;
int SC_Min = 0;
int SO_Hour = 12;
int SO_Min = 0;
int chooseSet = 0;
int chooseTimeBit = 0;
int fanPin = 6;
int fanInPin = A5;


bool doCMD(String cmd, String flag, long timeout) {
  String str = "";
  mySerial.println(cmd);    //发送AT指令
  delay(300);
  flag.toLowerCase();// 小写
  // 限期时间
  long deadline = millis() + timeout;
  while (millis() < deadline)
  {
    str = "";
    while (mySerial.available()) {
      str += (char)mySerial.read();
      //      delay(1);
    }
    if (str != "") {
      Serial.println(str);// 打印输出
      str.toLowerCase();// 小写
      // 判断 flag 是否需要切割，以 | 切割，逻辑或关系
      String flag1 = "", flag2 = "";
      int commaPosition = flag.indexOf('|');
      if (commaPosition > -1)
      {
        flag1 = flag.substring(0, commaPosition);
        flag2 = flag.substring(commaPosition + 1, flag.length());
      }
      if (flag1 != "" && flag2 != "") {
        if (str.indexOf(flag1) > -1 || str.indexOf(flag2) > -1 ) {
          return true;
        }
      }
      if (str.indexOf(flag) > -1) {
        return true;
      }
    }
    delay(100);
  }
  return false;
}
bool connectTCP() {
  bool linkOk = true;
  //连接服务器
  String s(F("AT+CIPSTART=\"TCP\",\"192.168.15.98\",6868"));// 字符串存入FLASH
  if (!doCMD(s, F("OK|CONNECTED"), 3000)) {
    linkOk = false;
  }
  if (linkOk) {
    doCMD(F("AT+CIPSEND=4"), F(">"), 10000);
    if (!doCMD(F("test"), SENDOK, 3000)) {
      linkOk = false;
    }
  }
  if (!linkOk) {
    Serial.println(F("!!SERVER NOT CONNECTED!!"));
  } else {
  }
  //  serverLinked = linkOk;
  return linkOk;
}
void printDateTime(const RtcDateTime& dt) {
  char datestring1[12];
  char datestring2[12];

  snprintf_P(datestring1,
             countof(datestring1),
             PSTR("%04u/%02u/%02u"),
             dt.Year(),
             dt.Month(),
             dt.Day());
  snprintf_P(datestring2,
             countof(datestring2),
             PSTR("%02u:%02u:%02u"),
             dt.Hour(),
             dt.Minute(),
             dt.Second());
  u8x8.println(datestring1);
  u8x8.println(datestring2);
}
void mark (unsigned int time) {
  TIMER_ENABLE_PWM; // Enable pin 3 PWM output
  if (time > 0) custom_delay_usec(time);
}
void space (unsigned int time) {
  TIMER_DISABLE_PWM; // Disable pin 3 PWM output
  if (time > 0) custom_delay_usec(time);
}
void custom_delay_usec(unsigned long uSecs) {
  if (uSecs > 4) {
    unsigned long start = micros();
    unsigned long endMicros = start + uSecs - 4;
    if (endMicros < start) { // Check if overflow
      while ( micros() > start ) {}
    }
    while ( micros() < endMicros ) {}
  }
}
void sendpresumable() {
  mark(8900);
  space(4450);
}
void send0() {
  mark(650);
  space(550);
}
void send1() {
  mark(650);
  space(1650);
}
void sendGree(byte ircode, byte len) {
  byte mask = 0x01;
  for (int i = 0; i < len; i++)
  {
    if (ircode & mask)
    {
      send1();
    }
    else
    {
      send0();
    }
    mask <<= 1;
  }
}
void closeAC() {
  TIMER_DISABLE_INTR; //Timer2 Overflow Interrupt
  pinMode(IRPin, OUTPUT);//PWM端口
  digitalWrite(IRPin, LOW); //PWM端口
  TIMER_CONFIG_KHZ(38);//PWM端口频率KHz

  sendpresumable();
  sendGree(0x51, 8);//000自动 100制冷 010除湿 110送风 001制热； 1开；00自动 10小风 01中风 11大风；0扫风；0睡眠//关
  sendGree(0x0A, 8);//温度0000-16度 1000-17度 1100-18度…. 0111-30度 低位在后
  sendGree(0x20, 8);
  sendGree(0x50, 8);
  sendGree(0x02, 3);
  mark(650);
  space(19700);
  sendGree(0x10, 8);
  sendGree(0x20, 8);
  sendGree(0x00, 8);
  sendGree(0x80, 8);

  mark(650);
  space(0);
}
void setAC() {
  TIMER_DISABLE_INTR; //Timer2 Overflow Interrupt
  pinMode(IRPin, OUTPUT);//PWM端口
  digitalWrite(IRPin, LOW); //PWM端口
  TIMER_CONFIG_KHZ(38);//PWM端口频率KHz
  int t;
  byte m, s;
  if (speed_AC == 0)
    s = 0x0;
  else if (speed_AC == 1)
    s = 0b00010000;
  else if (speed_AC == 2)
    s = 0b00100000;
  else if (speed_AC == 3)
    s = 0b00110000;
  if (mode_AC == 2)
    m = 0b00000100;
  else if (mode_AC == 1)
    m = 0b00000001;
  else if (mode_AC == 0)
    m = 0b00000000;
  t = temp_AC - 26;
  byte code1 = 0x40 | s | 0b00001000 | m;
  byte code2 = temp_AC - 16;
  byte code3 = t;
  Serial.println(code1, BIN);
  Serial.println(code2, BIN);
  Serial.println(code3, BIN);
  sendpresumable();
  sendGree(code1, 8);//000自动 100制冷 010除湿 110送风 001制热； 1开；00自动 10小风 01中风 11大风；0扫风；0睡眠//关
  sendGree(code2, 4); //温度0000-26度
  sendGree(0x0, 4);
  sendGree(0x20, 8);
  sendGree(0x50, 8);
  sendGree(0x02, 3);
  mark(650);
  space(19700);
  sendGree(0x10, 8);
  sendGree(0x20, 8);
  sendGree(0x00, 8);
  sendGree(0x00, 4);
  sendGree(code3, 4);

  mark(650);
  space(0);
}
void printTemp() {
  u8x8.print("Temprature: ");
  u8x8.println(temp_AC);
}
void printMode() {
  u8x8.print("Mode: ");
  if (mode_AC == 1)
    u8x8.println("Cooling");
  else if (mode_AC == 2)
    u8x8.println("Heating");
  else if (mode_AC == 0)
    u8x8.println("Auto");
}
void printSpeed() {
  u8x8.print("Speed: ");
  if (speed_AC == 1)
    u8x8.println("Low");
  else if (speed_AC == 2)
    u8x8.println("Medium");
  else if (speed_AC == 0)
    u8x8.println("Auto");
  else if (speed_AC == 3)
    u8x8.println("High");
}
void printTime(int hour, int min) {
  if (hour < 10)
    u8x8.print("0");
  u8x8.print(hour);
  u8x8.print(":");
  if (min < 10)
    u8x8.print("0");
  u8x8.println(min);
}
void printSetClose() {
  u8x8.print("Set Close: ");
  if (SC_AC == true) {
    u8x8.println("ON");
    printTime(SC_Hour, SC_Min);
  }
  else
    u8x8.println("OFF");
}
void printSetOpen() {
  u8x8.print("Set Open: ");
  if (SO_AC == true) {
    u8x8.println("ON");
    printTime(SO_Hour, SO_Min);
  }
  else
    u8x8.println("OFF");
}
void connectWIFI() {
  u8x8.clear();
  u8x8.println("WIFI");
  u8x8.println("Connecting...");
  doCMD(F("AT+RST"), F("OK"), 2000);
  doCMD(F("AT+CWMODE=1"), F("OK"), 2000);
  doCMD(F("AT+CWJAP_DEF=\"somnus\",\"somnusym\""), F("GOT"), 50000);
  delay(2000);
  connectTCP();
  u8x8.println("WIFI ready!");
  delay(300);
  u8x8.clear();
}

void setup () {
  u8x8.begin();
  u8x8.clear();
  u8x8.clearDisplay();
  u8x8.setFlipMode(1);
  u8x8.setFont(u8x8_font_7x14_1x2_r);
  u8x8.setCursor(0, 0);
  Serial.begin(57600);
  while (!Serial) {
    ;
  }
  mySerial.begin(115200);
  while (!mySerial) {
    ;
  }
  connectWIFI();
  pinMode(rotaryPin, INPUT);
  Rtc.Begin();
  Rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__)); //修正DS1302存储的时间
  Wire.begin();
  u8x8.clearDisplay();
  pinMode(ledPin, OUTPUT);
  pinMode(fanPin, OUTPUT);
  pinMode(fanInPin, OUTPUT);
  digitalWrite(fanInPin, HIGH);
}

void loop () {
  // wifi软串口发送过来的信息
  String str = "";
  while (mySerial.available()) {
    str = str + (char)mySerial.read();
  }
  if (str != "") {
    Serial.println(str);
    if (str.indexOf("setAC") != -1 || str.indexOf("openAC") != -1) {
      setAC();
      state_AC = true;
      mySerial.println("Set OK!");
    }
    else if (str.indexOf("closeAC") != -1) {
      closeAC();
      state_AC = false;
    }
    else if (str.indexOf("openLight") != -1 || str.indexOf("ol") != -1) {
      digitalWrite(ledPin, HIGH);
      state_LIGHT = true;
    }
    else if (str.indexOf("closeLight") != -1 || str.indexOf("cl") != -1) {
      digitalWrite(ledPin, LOW);
      state_LIGHT = false;
    }
    else if (str.indexOf("openFan") != -1 || str.indexOf("of") != -1) {
      digitalWrite(fanPin, HIGH);
      state_FAN = true;
    }
    else if (str.indexOf("closeFan") != -1 || str.indexOf("cf") != -1) {
      digitalWrite(fanPin, LOW);
      state_FAN = true;
    }
    else {
      int temp = str.indexOf("temp:");
      String s;
      if (temp != -1) {
        s = str.substring(temp + 5, str.length());
        temp_AC = atoi(s.c_str());
        setAC();
      }
      temp = str.indexOf("mode:");
      if (temp != -1) {
        s = str.substring(temp + 5, str.length());
        mode_AC = atoi(s.c_str());
        setAC();
      }
      temp = str.indexOf("speed:");
      if (temp != -1) {
        s = str.substring(temp + 6, str.length());
        speed_AC = atoi(s.c_str());
        setAC();
      }
    }
  }
  int r = analogRead(rotaryPin);
  if (r < rotaryValue - 20 || r > rotaryValue + 20) {
    u8x8.clearDisplay();
    choose = 0;
    ct = 0;
  }
  rotaryValue = r;
  t = Rtc.GetDateTime();

  if ( SC_AC == true && t.Hour() == SC_Hour && t.Minute() == SC_Min) {
    closeAC();
    state_AC = false;
    SC_AC = false;
    u8x8.clear();
  }
  if ( SO_AC == true && t.Hour() == SO_Hour && t.Minute() == SO_Min) {
    setAC();
    state_AC = true;
    SO_AC = false;
    u8x8.clear();
  }
  if (r < 256) {
    char customKey = customKeypad.getKey();
    if (customKey) Serial.println(customKey);
    switch (customKey) {
      case '1': {
          //控制点灯
          if (state_LIGHT == false)
            digitalWrite(ledPin, HIGH);
          else
            digitalWrite(ledPin, LOW);
          state_LIGHT = !state_LIGHT;
          break;
        }
      case '0': {
          //开关空调
          if (state_AC == false)
            setAC();
          else
            closeAC();
          state_AC = !state_AC;
          break;
        }
      case '2': {
          //继电器控制风扇
          //控制点灯
          if (state_FAN == false)
            digitalWrite(fanPin, HIGH);
          else
            digitalWrite(fanPin, LOW);
          state_FAN = !state_FAN;
          //        Rtc.SetDateTime(t);
          break;
        }
      case '3': {
          connectWIFI();
        }
    }

    DHT11.read(DHT11PIN);
    t = Rtc.GetDateTime();
    // 显示
    u8x8.setCursor(0, 0);

    printDateTime(t);
    u8x8.print("Temp: ");
    u8x8.println((float)DHT11.temperature, 1);
    u8x8.print("Humi: ");
    u8x8.println((float)DHT11.humidity, 1);
  }
  else if (r < 512) {
    //    u8x8.clearDisplay();
    u8x8.setCursor(0, 0);

    Serial.println(state_AC);
    Serial.println(temp_AC);
    Serial.println(mode_AC);
    Serial.println(speed_AC);
    char customKey = customKeypad.getKey();
    if (customKey) Serial.println(customKey);
    switch (customKey) {
      case '1': {
          //控制点灯
          if (state_LIGHT == false)
            digitalWrite(ledPin, HIGH);
          else
            digitalWrite(ledPin, LOW);
          state_LIGHT = !state_LIGHT;
          break;
        }
      case '0': {
          //开关空调
          if (state_AC == false)
            setAC();
          else
            closeAC();
          state_AC = !state_AC;
          break;
        }
      case '2': {
          //继电器控制风扇
          if (state_FAN == false)
            digitalWrite(fanPin, HIGH);
          else
            digitalWrite(fanPin, LOW);
          state_FAN = !state_FAN;
          //        Rtc.SetDateTime(t);
          break;
        }
    }

    u8x8.print("AC: ");
    if (state_AC == false)
      u8x8.println("OFF");
    else
      u8x8.println("ON");

    u8x8.print("LIGHT: ");
    if (state_LIGHT == false)
      u8x8.println("OFF");
    else
      u8x8.println("ON");

    u8x8.print("FAN: ");
    if (state_FAN == false)
      u8x8.println("OFF");
    else
      u8x8.println("ON");
  }
  else if (r < 768) {
    u8x8.setCursor(0, 0);
    if (ct == 0) ct = millis();
    if (choose == 0) {
      u8x8.print("AC: ");
      if (state_AC == false)
        u8x8.println("OFF");
      else
        u8x8.println("ON");
      printTemp();
      printMode();
      printSpeed();

      char customKey = customKeypad.getKey();
      if (customKey) Serial.println(customKey);
      switch (customKey) {
        case '0': {
            // 发送setAC
            setAC();
            break;
          }
        case '1': {
            choose++;
            choose %= 4;
            ct = millis();
            u8x8.clear();
            break;
          }
      }
    }
    else if (choose == 1) {
      printTemp();
      char customKey = customKeypad.getKey();
      if (customKey) Serial.println(customKey);
      switch (customKey) {
        case '0': {
            choose = 0;
            break;
          }
        case '1': {
            choose++;
            choose %= 4;
            ct = millis();
            u8x8.clear();
            break;
          }
        case '2': {
            if (temp_AC - 1 >= 16)
              temp_AC--;
            ct = millis();
            break;
          }
        case '3': {
            if (temp_AC + 1 <= 30)
              temp_AC++;
            ct = millis();
            break;
          }
      }

      if (millis() - ct >= 1500) {
        setAC();
        choose = 0;
        u8x8.clear();
      }
    }
    else if (choose == 2) {
      printMode();

      char customKey = customKeypad.getKey();
      if (customKey) Serial.println(customKey);
      switch (customKey) {
        case '0': {
            choose = 0;
            u8x8.clear();
            break;
          }
        case '1': {
            choose++;
            choose %= 4;
            ct = millis();
            u8x8.clear();
            break;
          }
        case '2': {
            if (mode_AC - 1 >= 0)
              mode_AC--;
            ct = millis();
            u8x8.clear();
            printMode();
            break;
          }
        case '3': {
            if (mode_AC + 1 <= 2)
              mode_AC++;
            ct = millis();
            u8x8.clear();
            printMode();
            break;
          }
      }

      if (millis() - ct >= 1500) {
        setAC();
        choose = 0;
        u8x8.clear();
      }
    }
    else if (choose == 3) {
      printSpeed();

      char customKey = customKeypad.getKey();
      if (customKey) Serial.println(customKey);
      switch (customKey) {
        case '0': {
            choose = 0;
            u8x8.clear();
            break;
          }
        case '1': {
            choose++;
            choose %= 4;
            ct = millis();
            u8x8.clear();
            break;
          }
        case '2': {
            if (speed_AC - 1 >= 0)
              speed_AC--;
            ct = millis();
            u8x8.clear();
            printSpeed();
            break;
          }
        case '3': {
            if (speed_AC + 1 <= 3)
              speed_AC++;
            ct = millis();
            u8x8.clear();
            printSpeed();
            break;
          }
      }

      if (millis() - ct >= 1500) {
        setAC();
        choose = 0;
        u8x8.clear();
      }
    }
  }
  else {
    u8x8.setCursor(0, 0);

    if (chooseSet == 0) {
      printSetClose();
      printSetOpen();

      char customKey = customKeypad.getKey();
      if (customKey) Serial.println(customKey);
      switch (customKey) {
        case '0': {
            chooseSet = 1;
            u8x8.clear();
            break;
          }
        case '1': {
            chooseSet = 2;
            u8x8.clear();
            break;
          }
      }
    }
    else if (chooseSet == 1) {
      if (ct == 0) ct = millis();
      printSetClose();
      if (SC_AC == true) {
        if (chooseTimeBit == 0) {
          u8x8.print("~~");
        }
        else {
          u8x8.print("   ~~");
        }
      }
      char customKey = customKeypad.getKey();
      if (customKey) Serial.println(customKey);
      switch (customKey) {
        case '0': {
            SC_AC = !SC_AC;
            ct = millis();
            u8x8.clear();
            printSetOpen();
            break;
          }
        case '1': {
            chooseTimeBit++;
            chooseTimeBit %= 2;
            ct = millis();
            u8x8.clear();
            break;
          }
        case '2': {
            if (chooseTimeBit == 0) {
              if ( SC_Hour >= 1)
                SC_Hour--;
            }
            else {
              if ( SC_Min >= 1) {
                SC_Min--;
              }
              else if (SC_Hour >= 1) {
                SC_Min = 59;
                SC_Hour--;
              }
            }
            ct = millis();
            break;
          }
        case '3': {
            if (chooseTimeBit == 0) {
              if (SC_Hour <= 22)
                SC_Hour++;
            }
            else {
              if (SC_Min <= 58) {
                SC_Min++;
              }
              else if (SC_Hour <= 22) {
                SC_Hour++;
                SC_Min = 0;
              }
            }
            ct = millis();
            break;
          }
      }
      if (millis() - ct >= 2500) {
        chooseSet = 0;
        u8x8.clear();
        ct = millis();
      }
    }
    else {
      if (ct == 0) ct = millis();
      printSetOpen();
      if (SO_AC == true) {
        if (chooseTimeBit == 0) {
          u8x8.println("~~");
        }
        else
          u8x8.println("   ~~");
      }
      char customKey = customKeypad.getKey();
      if (customKey) Serial.println(customKey);
      switch (customKey) {
        case '0': {
            SO_AC = !SO_AC;
            u8x8.clear();
            printSetOpen();
            ct = millis();
            break;
          }
        case '1': {
            chooseTimeBit++;
            chooseTimeBit %= 2;
            u8x8.clear();
            ct = millis();
            break;
          }
        case '2': {
            if (chooseTimeBit == 0) {
              if ( SO_Hour >= 1)
                SO_Hour--;
            }
            else {
              if ( SO_Min >= 1) {
                SO_Min--;
              }
              else if (SO_Hour >= 1) {
                SO_Min = 59;
                SO_Hour--;
              }
            }
            ct = millis();
            break;
          }
        case '3': {
            if (chooseTimeBit == 0) {
              if (SO_Hour <= 22)
                SO_Hour++;
            }
            else {
              if (SO_Min <= 58) {
                SO_Min++;
              }
              else if (SO_Hour <= 22) {
                SO_Hour++;
                SO_Min = 0;
              }
            }
            ct = millis();
            break;
          }
      }
      if (millis() - ct >= 2500) {
        chooseSet = 0;
        u8x8.clear();
        ct = millis();
      }
    }
  }
}
