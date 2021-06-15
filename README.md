# RoomPanel
 Arduino实现的室内环境中控板

使用的硬件：
Seeeduino Lotus（Grove Beginner Kit套件中的板卡），与UNO很类似，只是集成了一些模块，比如本程序所用到的，SSD1315 128*64 0.96寸OLED显示屏和旋钮。
DHT11温湿度传感器
ESP-01S ESP8266 WIFI模块
四键按钮模块
DS1032 时钟模块
两路5V继电器模块
红外发射管模块/红外发射管LED

使用到的Arduino库：
IRremote：一般的红外库，但我只用来测试发送的红外信号，实际发送的实现是使用Timer2的PWM信号发送和自行编码实现。
Keypad：四键按钮所用库
Rtc By Makuna：DS1302时钟所用库
U8g2：使用了此库里的u8x8来写显示屏，原因是占用内存小。
Dht11：温湿度传感器库，没有用Grove自带的，原因是Grove的读取速度很慢，影响程序的Loop循环。
