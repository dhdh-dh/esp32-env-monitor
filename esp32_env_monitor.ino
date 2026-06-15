#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <Wire.h>
#include <SSD1306Wire.h>
#include"config.h"


SSD1306Wire display(0x3c, 21, 22);

DHT dht(DHT_PIN, DHT_TYPE);



bool wifiConnected = false;
bool lastSendStatus = false;
unsigned long lastDataSend = 0;


void setup() {
  Serial.begin(115200);
  
  if (!display.init()) {
    Serial.println("OLED初始化失败!");
    while(1);
  }
  display.flipScreenVertically();
  
  dht.begin();
  connectToWiFi();
  
  Serial.println("=== 环境监测系统已启动 ===");
  Serial.println("上传间隔: " + String(SEND_INTERVAL/60000) + "分钟");
}

void loop() {
  // 读取传感器数据
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  // 更新WiFi状态
  wifiConnected = (WiFi.status() == WL_CONNECTED);
  
  // 在OLED上显示所有信息
  displayOLED(temperature, humidity);
  
  // 每5分钟发送数据到ThingSpeak
  unsigned long currentTime = millis();
  unsigned long timeSinceLastSend;
  
  // 正确处理时间差（包含溢出保护）
  if (currentTime >= lastDataSend) {
    timeSinceLastSend = currentTime - lastDataSend;
  } else {
    timeSinceLastSend = (0xFFFFFFFF - lastDataSend) + currentTime;
  }
  
  if (wifiConnected && timeSinceLastSend >= SEND_INTERVAL) {
    if (!isnan(temperature) && !isnan(humidity)) {
      lastSendStatus = sendToThingSpeak(temperature, humidity);
      lastDataSend = currentTime;
    }
  }
  
  // 如果WiFi断开则尝试重连
  if (!wifiConnected) {
    connectToWiFi();
  }
  
  delay(2000);
}

void displayOLED(float temp, float humi) {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  
  // 第一行：温湿度数据
  if (isnan(temp) || isnan(humi)) {
    display.drawString(0, 0, "传感器错误");
  } else {
    display.drawString(0, 0, "T:" + String(temp, 1) + "C");
    display.drawString(64, 0, "H:" + String(humi, 1) + "%");
  }
  
  // 第二行：WiFi状态和IP地址
  String wifiStatus = wifiConnected ? "W:OK" : "W:NO";
  display.drawString(0, 12, wifiStatus);
  
  if (wifiConnected) {
    String ip = WiFi.localIP().toString();
    int lastDot = ip.lastIndexOf('.');
    if (lastDot != -1) {
      display.drawString(30, 12, "IP:" + ip.substring(lastDot + 1));
    }
  }
  
  // 第三行：云端状态
  String cloudStatus = lastSendStatus ? "C:OK" : "C:NO";
  display.drawString(0, 24, cloudStatus);
  
  // 第四行：下次发送时间（修复版本）
  unsigned long currentTime = millis();
  unsigned long timeSinceLastSend;
  
  // 正确处理时间差
  if (currentTime >= lastDataSend) {
    timeSinceLastSend = currentTime - lastDataSend;
  } else {
    timeSinceLastSend = (0xFFFFFFFF - lastDataSend) + currentTime;
  }
  
  int minutesUntilSend;
  if (timeSinceLastSend < SEND_INTERVAL) {
    minutesUntilSend = (SEND_INTERVAL - timeSinceLastSend) / 60000;
  } else {
    minutesUntilSend = 0;
  }
  
  // 防止显示异常值
  if (minutesUntilSend > 10) minutesUntilSend = 5;
  
  display.drawString(0, 36, "N:" + String(minutesUntilSend) + "m");
  
  // 第五行：项目标识
  display.drawString(0, 48, "Env Monitor");
  
  display.display();
}

bool sendToThingSpeak(float temp, float humi) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    String url = THINGSPEAK_URL+ "?api_key=" + THINGSPEAK_API_KEY + 
                 "&field1=" + String(temp, 1) + 
                 "&field2=" + String(humi, 1);
    
    Serial.println("发送数据到ThingSpeak...");
    
    http.begin(url);
    int httpCode = http.GET();
    
    bool success = (httpCode == 200);
    
    if (success) {
      Serial.println("✅ 数据发送成功!");
    } else {
      Serial.println("❌ 发送失败, HTTP代码: " + String(httpCode));
    }
    
    http.end();
    return success;
  }
  return false;
}

void connectToWiFi() {
  Serial.println("连接WiFi: " + String(WIFI_SSID));
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\n✅ WiFi连接成功!");
    Serial.println("IP地址: " + WiFi.localIP().toString());
  } else {
    wifiConnected = false;
    Serial.println("\n❌ WiFi连接失败!");
  }
}