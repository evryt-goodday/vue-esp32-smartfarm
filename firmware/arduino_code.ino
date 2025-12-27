#include <DHT.h>
#include <Adafruit_NeoPixel.h> 
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ==========================================
// 사용자 설정 구역
// ==========================================
const char* ssid = "DEV";       // WiFi 이름
const char* pass = "54865486";   // WiFi 비밀번호

// 서버 설정 (PROJECT_SETUP.md 기준)
// IP 주소는 실제 서버 환경에 맞게 수정 필요
const char* serverUrl = "http://192.168.0.133:3002/api/sensor/data"; 
const char* controlUrl = "http://192.168.0.133:3002/api/sensor/control/"; // 뒤에 ID 붙여서 호출

// 장치 ID (DB와 일치)
const int ACTUATOR_ID_FAN = 1;
const int ACTUATOR_ID_PUMP = 2;
const int ACTUATOR_ID_LED = 3;

const int SENSOR_ID_TEMP = 1;
const int SENSOR_ID_HUMI = 2;
const int SENSOR_ID_SOIL = 3;

// ==========================================

// 하드웨어 핀 설정
// ==========================================
#define DHTTYPE DHT11
#define DHTPIN 25
DHT dht(DHTPIN, DHTTYPE);

#define MOTOR_A1 16 // FAN
#define MOTOR_A2 17 // FAN
#define MOTOR_B1 18 // WATER PUMP
#define MOTOR_B2 19 // WATER PUMP
#define SOIL 36 

#define PIN 26          
#define NUMPIXELS 9     
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

#define BUTTON 23       
#define CDS 39          

#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define OLED_RESET     -1 
#define SCREEN_ADDRESS 0x3C 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ==========================================
// 전역 변수
// ==========================================
// 센서 값
float humi = 0;
float temp = 0;
int soil_value = 0;
int cds_value = 0;

// 제어 모드 (장치별 개별 모드)
bool isFanAuto = true;
bool isPumpAuto = true;
bool isLedAuto = true;

// 액추에이터 현재 상태 (실제 작동 상태)
bool fan_state = false;
bool pump_state = false;
bool led_state = false;

// 수동 제어 목표 상태 (서버에서 수신)
bool target_fan_state = false;
bool target_pump_state = false;
bool target_led_state = false;

// 타이머 및 버튼 처리 변수
unsigned long lastTime = 0;
const unsigned long timerDelay = 1000; // 1초 주기
int lastButtonState = LOW;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// ==========================================
// 함수 선언
// ==========================================
void connectWiFi();
void readSensors();
void checkButton();
void fetchCommands();
void controlLogic();
void applyActuators();
void updateDisplay();
void sendDataToServer();
void getControlState(int actuatorId, bool &targetState, bool &isAuto);

void setup() {
  Serial.begin(9600);

  // 핀 모드 설정
  pinMode(MOTOR_A1, OUTPUT);
  pinMode(MOTOR_A2, OUTPUT);
  pinMode(MOTOR_B1, OUTPUT);
  pinMode(MOTOR_B2, OUTPUT);
  pinMode(BUTTON, INPUT);
  
  pixels.begin();
  dht.begin();

  // 디스플레이 초기화
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Booting...");
  display.display();

  // WiFi 연결
  connectWiFi();
}

void loop() {
  // WiFi 연결 상태 확인 및 재연결
  if(WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // 버튼 상태 확인 (모드 전환) - 항상 체크
  checkButton();

  // 주기적 작업 (1초마다)
  if ((millis() - lastTime) > timerDelay) {
    lastTime = millis();

    // 1. 센서 값 읽기
    readSensors();

    // 2. 서버 명령 확인 (Manual 모드일 때 사용할 타겟 상태 갱신)
    if(WiFi.status() == WL_CONNECTED) {
      fetchCommands();
    }

    // 3. 제어 로직 판단 (Auto/Manual)
    controlLogic();

    // 4. 하드웨어 구동
    applyActuators();

    // 5. 디스플레이 갱신
    updateDisplay();

    // 6. 데이터 전송
    if(WiFi.status() == WL_CONNECTED) {
      sendDataToServer();
    }
  }
}

// ==========================================
// 주요 기능 구현
// ==========================================

void connectWiFi() {
  Serial.print("Connecting to ");
  Serial.println(ssid);
  display.clearDisplay();
  display.setCursor(0,0);
  display.print("Connecting WiFi...");
  display.display();

  WiFi.begin(ssid, pass);
  
  int tryCount = 0;
  while (WiFi.status() != WL_CONNECTED && tryCount < 10) {
    delay(500);
    Serial.print(".");
    tryCount++;
  }
  
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi Connection Failed");
  }
}

void readSensors() {
  // DHT11
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  
  if (!isnan(h)) humi = h;
  if (!isnan(t)) temp = t;

  // Soil Moisture
  // 3600(건조) ~ 1200(습함) -> 0 ~ 100% 매핑
  int rawSoil = analogRead(SOIL);
  soil_value = map(rawSoil, 3600, 1200, 0, 100);
  soil_value = constrain(soil_value, 0, 100);

  // CDS (조도)
  cds_value = analogRead(CDS);
}

void checkButton() {
  int reading = digitalRead(BUTTON);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // 버튼이 눌렸을 때 (HIGH) -> 전체 모드 토글 (편의 기능)
    // 실제로는 웹에서 개별 제어하지만, 물리 버튼은 전체 일괄 전환용으로 사용
    static int buttonState;
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == HIGH) {
        bool nextMode = !isFanAuto; // 팬 기준으로 토글
        isFanAuto = nextMode;
        isPumpAuto = nextMode;
        isLedAuto = nextMode;
        Serial.print("All Mode Changed: ");
        Serial.println(nextMode ? "AUTO" : "MANUAL");
      }
    }
  }
  lastButtonState = reading;
}

void fetchCommands() {
  // 서버에서 각 장치의 타겟 상태 및 모드 조회 (Polling)
  getControlState(ACTUATOR_ID_FAN, target_fan_state, isFanAuto);
  getControlState(ACTUATOR_ID_PUMP, target_pump_state, isPumpAuto);
  getControlState(ACTUATOR_ID_LED, target_led_state, isLedAuto);
}

void controlLogic() {
  // 1. Fan Control Logic
  if (isFanAuto) {
    if (humi > 70) fan_state = true;
    else fan_state = false;
  } else {
    fan_state = target_fan_state;
  }

  // 2. Pump Control Logic
  if (isPumpAuto) {
    if (soil_value < 20) pump_state = true;
    else pump_state = false;
  } else {
    pump_state = target_pump_state;
  }

  // 3. LED Control Logic
  if (isLedAuto) {
    if (cds_value < 200) led_state = true;
    else led_state = false;
  } else {
    led_state = target_led_state;
  }
}

void applyActuators() {
  // Fan Control
  if (fan_state) {
    digitalWrite(MOTOR_A1, LOW);
    digitalWrite(MOTOR_A2, HIGH);
  } else {
    digitalWrite(MOTOR_A1, LOW);
    digitalWrite(MOTOR_A2, LOW);
  }

  // Pump Control
  if (pump_state) {
    digitalWrite(MOTOR_B1, HIGH);
    digitalWrite(MOTOR_B2, LOW);
  } else {
    digitalWrite(MOTOR_B1, LOW);
    digitalWrite(MOTOR_B2, LOW);
  }

  // LED Control (NeoPixel)
  if (led_state) {
    for(int i=0; i<NUMPIXELS; i++) {
      if(i%2 == 0) pixels.setPixelColor(i, pixels.Color(150, 0, 0)); // Red
      else pixels.setPixelColor(i, pixels.Color(0, 0, 150)); // Blue
    }
  } else {
    pixels.clear();
  }
  pixels.show();
}

void updateDisplay() {
  display.clearDisplay();
  
  // 상단: 제목
  display.setCursor(0,0);
  display.print("SMART FARM");
  
  // 센서 값
  display.setCursor(0,15);
  display.print("Temp:"); display.print((int)temp); display.print("C ");
  display.print("Humi:"); display.print((int)humi); display.print("%");
  
  display.setCursor(0,25);
  display.print("Soil: "); display.print(soil_value); display.print(" %");
  
  // 장치 상태 + 모드 (F:Fan, P:Pump, L:LED)
  // 형식: F:ON(A) = Fan ON, Auto Mode
  display.setCursor(0,40);
  display.print("F:"); 
  display.print(fan_state ? "ON" : "OFF");
  display.print(isFanAuto ? "(A)" : "(M)");
  
  display.setCursor(0,50);
  display.print("P:"); 
  display.print(pump_state ? "ON" : "OFF");
  display.print(isPumpAuto ? "(A)" : "(M)");
  display.print(" L:"); 
  display.print(led_state ? "ON" : "OFF");
  display.print(isLedAuto ? "(A)" : "(M)");
  
  display.display();
}

// ==========================================
// 서버 통신 함수
// ==========================================

void sendDataToServer() {
  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");
  
  // JSON 생성: 백엔드 saveSensorData에서 받는 필드명과 일치해야 함
  String requestBody = "{";
  requestBody += "\"temp\":" + String(temp) + ",";
  requestBody += "\"humi\":" + String(humi) + ",";
  requestBody += "\"soil\":" + String(soil_value) + ",";
  requestBody += "\"fan_state\":" + String(fan_state ? 1 : 0) + ",";
  requestBody += "\"pump_state\":" + String(pump_state ? 1 : 0) + ",";
  requestBody += "\"led_state\":" + String(led_state ? 1 : 0) + ",";
  requestBody += "\"fan_auto\":" + String(isFanAuto ? 1 : 0) + ",";
  requestBody += "\"pump_auto\":" + String(isPumpAuto ? 1 : 0) + ",";
  requestBody += "\"led_auto\":" + String(isLedAuto ? 1 : 0);
  requestBody += "}";
  
  int httpResponseCode = http.POST(requestBody);
  
  if (httpResponseCode > 0) {
    // Serial.print("Data Sent: "); Serial.println(httpResponseCode);
  } else {
    Serial.print("Error Sending Data: "); Serial.println(httpResponseCode);
  }
  http.end();
}

void getControlState(int actuatorId, bool &targetState, bool &isAuto) {
  HTTPClient http;
  String url = String(controlUrl) + String(actuatorId);
  http.begin(url);
  
  int httpResponseCode = http.GET();
  
  if (httpResponseCode == 200) {
    String payload = http.getString();
    
    // JSON 파싱: {"target_state": 1, "is_auto_mode": 0}
    // ArduinoJson 7.x 문법
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      targetState = doc["target_state"];
      isAuto = doc["is_auto_mode"];
      Serial.print("Device "); Serial.print(actuatorId);
      Serial.print(" -> Target: "); Serial.print(targetState);
      Serial.print(", Auto: "); Serial.println(isAuto);
    } else {
      Serial.print("JSON Parse Error: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.print("HTTP Error (Device "); Serial.print(actuatorId); 
    Serial.print("): "); Serial.println(httpResponseCode);
  }
  
  http.end();
}