/**
 * ESP32-CAM Teachable Machine
 * 
 * 連線模式切換：取消下方其中一行的註解來選擇模式（只能選擇一種）
 *   WIFI_AP_MODE  — ESP32 作為基地台，手機直接連接（固定 IP 192.168.4.1）
 *   WIFI_STA_MODE — ESP32 連線到現有 WiFi 路由器（IP 由 DHCP 指派）
 */

// === 請在此選擇連線模式（二選一）===
//#define WIFI_AP_MODE
#define WIFI_STA_MODE

/*
http://192.168.xxx.xxx             //網頁首頁管理介面
http://192.168.xxx.xxx/capture     //取得單張 JPEG 影像
http://192.168.xxx.xxx/status      //取得視訊參數值

自訂指令格式 :  
http://IP/control?cmd=P1;P2;P3;P4;P5;P6;P7;P8;P9
http://IP/control?var=<variable>&val=<value>
http://IP/capture         # 取得單張影像
http://IP/status           # 取得目前設定值 (JSON)
http://IP/control?ip       # 取得 IP 位址
http://IP/control?mac      # 取得 MAC 位址
http://IP/control?flash=value             # 內建閃光燈 value= 0~255
http://IP/control?digitalwrite=pin;value  # 數位輸出
http://IP/control?analogwrite=pin;value   # 類比輸出
http://IP/control?digitalread=pin         # 數位讀取
http://IP/control?analogread=pin          # 類比讀取
http://IP/control?touchread=pin           # 觸碰讀取
http://IP/control?resetwifi=ssid;password # 重設 Wi-Fi（僅 AP 模式）

官方指令格式 http://IP/control?var=***&val=***
http://IP/control?var=framesize&val=value
http://IP/control?var=quality&val=value
http://IP/control?var=brightness&val=value
http://IP/control?var=contrast&val=value
http://IP/control?var=hmirror&val=value
http://IP/control?var=vflip&val=value
http://IP/control?var=flash&val=value

查詢 IP：http://IP/?ip
*/
// Select camera model
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
//#define CAMERA_MODEL_M5STACK_UNITCAM // No PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM// Select camera model

#include "camera_pins.h"

// === 硬體腳位定義與控制參數 ===
#include <ESP32Servo.h>

// 偵測結果 LED 指示
#define LED1_PIN 12   //PET 偵測時點亮
#define LED2_PIN 13   //CAN 偵測時點亮
#define SERVO_PIN 14  //PET→servoPosMax, CAN→servoPosMin

// 舵機位置（角度 0~180）
const int servoPosCenter = 90;
const int servoPosMax    = 90 + 35;
const int servoPosMin    = 90 - 35;
const int servoStopTime  = 1000;  // 到達定位後停留時間 (ms)
const int servoDelayTime = 1000;  // 回到中心後等待時間 (ms)

// 辨識觸發門檻
float acceptanceRate = 0.7;  // 信心度大於此值才觸發舵機

// 預測週期（ms）
const int predictInterval = 1000;  // 預測迴圈間隔時間

// 硬體狀態機
enum HwState { HW_STARTUP, HW_IDLE, HW_TRIGGERED, HW_AT_POS, HW_RETURN };
HwState hwState = HW_STARTUP;
unsigned long hwTimer = 0;
String hwClass = "";         // 當前觸發的類別 ("PET" / "CAN")
int hwTargetPos = servoPosCenter;
bool hwStartupDone = false;

// 由 HTTP handler 寫入、loop() 讀取的偵測結果佇列
String pendingClass = "";
float pendingProb = 0.0;
bool pendingDetection = false;
String lastTriggeredClass = "";  // 鎖定：同一類別不重複觸發，直到物體離開
Servo myservo;

// === WiFi 連線模式 ===
#ifdef WIFI_AP_MODE
// AP 模式設定
const char* apssid = "KTS_SmartBin_AP";
const char* appassword = "12345678";
#elif defined(WIFI_STA_MODE)
// STA 模式設定（連線到現有 WiFi 路由器）
const char* sta_ssid = "Smile 3A";
const char* sta_password = "3a2l20220303";
#else
#error "Please define WIFI_AP_MODE or WIFI_STA_MODE"
#endif

#include <WiFi.h>
#include <esp32-hal-ledc.h>      //用於控制伺服馬達
#include "soc/soc.h"             //用於電源不穩不重開機 
#include "soc/rtc_cntl_reg.h"    //用於電源不穩不重開機
//官方函式庫
#include "esp_camera.h"          //視訊函式庫
#include "esp_http_server.h"     //HTTP Server函式庫
#include "img_converters.h"      //影像格式轉換函式庫

String Feedback="";   //自訂指令回傳客戶端訊息

//自訂指令參數值
String Command="";
String cmd="";
String P1="";
String P2="";
String P3="";
String P4="";
String P5="";
String P6="";
String P7="";
String P8="";
String P9="";

//自訂指令拆解狀態值
byte ReceiveState=0;
byte cmdState=1;
byte strState=1;
byte questionstate=0;
byte equalstate=0;
byte semicolonstate=0;

typedef struct {
        httpd_req_t *req;
        size_t len;
} jpg_chunking_t;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;
bool ledBlink = false;

// 前向宣告（Arduino IDE 因 raw string literal 無法自動產生 prototype）
void startCameraServer();
void getCommand(char c);

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  //關閉電源不穩就重開機的設定   
  Serial.begin(115200);
  Serial.setDebugOutput(true);  //開啟診斷輸出
  Serial.println();
  //視訊組態設定  https://github.com/espressif/esp32-camera/blob/master/driver/include/esp_camera.h
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  //
  // WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
  //            Ensure ESP32 Wrover Module or other board with PSRAM is selected
  //            Partial images will be transmitted if image exceeds buffer size
  //   
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(psramFound()){  //是否有PSRAM(Psuedo SRAM)記憶體IC
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  //視訊初始化
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }

  //可自訂視訊框架預設大小(解析度大小)
  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_QQVGA);    //預設解析度 QQVGA(160x120)，改善流暢度

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);  //垂直翻轉
  s->set_hmirror(s, 1);  //水平鏡像
#endif

#if defined(CAMERA_MODEL_AI_THINKER)
  //閃光燈(GPIO4)
  ledcAttach(4, 5000, 8);
#endif
  
  // === WiFi 連線（依 WIFI_AP_MODE / WIFI_STA_MODE 二選一）===
#ifdef WIFI_AP_MODE
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP((String)apssid, appassword);
  delay(500);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
#elif defined(WIFI_STA_MODE)
  WiFi.mode(WIFI_STA);
  WiFi.begin(sta_ssid, sta_password);
  Serial.print("Connecting to WiFi");
  unsigned long staStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - staStart < 10000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("STA IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("STA connection failed, check ssid/password");
  }
#endif
  Serial.println("");

  startCameraServer();

#if defined(CAMERA_MODEL_AI_THINKER)
  // 初始化硬體控制
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);
  myservo.attach(SERVO_PIN);
  
  // 開機校準：中心→最大→退回中心，確認舵機正常
  Serial.println("Servo calibration start...");
  myservo.write(servoPosCenter);
  delay(servoStopTime);
  myservo.write(servoPosMax);
  delay(servoStopTime);
  myservo.write(servoPosMin);
  delay(servoStopTime);
  myservo.write(servoPosCenter);
  delay(servoStopTime);
  Serial.println("Servo calibration done");
  
  // 進入待機狀態
  digitalWrite(LED1_PIN, HIGH);
  digitalWrite(LED2_PIN, HIGH);
  hwState = HW_IDLE;
  hwStartupDone = true;
  
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);
#endif
}

void loop() {
#if defined(CAMERA_MODEL_AI_THINKER)
  // LED 慢閃（模型載入中）
  static unsigned long lastBlink = 0;
  static bool ledState = false;
  if (ledBlink) {
    unsigned long now = millis();
    if (now - lastBlink >= 1000) {
      ledState = !ledState;
      digitalWrite(4, ledState ? HIGH : LOW);
      lastBlink = now;
    }
  }

  // 硬體狀態機（非阻塞）
  if (hwStartupDone) {
    // 處理每個新的偵測結果（無論是否鎖定，都更新類別判斷）
    if (pendingDetection) {
      pendingDetection = false;
      String cls = pendingClass;
      float prob = pendingProb;

      if (hwState == HW_IDLE) {
        if (lastTriggeredClass != "") {
          // 鎖定中：檢查物體是否已離開（改為其他類別或信心度低於門檻）
          if (cls != lastTriggeredClass || prob < acceptanceRate) {
            lastTriggeredClass = "";
          }
        }
        if (lastTriggeredClass == "") {
          // 未鎖定：檢查是否觸發新的偵測
          if (cls == "PET" && prob > acceptanceRate) {
            hwClass = "PET";
            hwTargetPos = servoPosMax;
            hwState = HW_TRIGGERED;
            lastTriggeredClass = "PET";
          } else if (cls == "CAN" && prob > acceptanceRate) {
            hwClass = "CAN";
            hwTargetPos = servoPosMin;
            hwState = HW_TRIGGERED;
            lastTriggeredClass = "CAN";
          }
        }
      }
    }
    switch (hwState) {
      case HW_TRIGGERED:
        if (hwClass == "PET") {
          digitalWrite(LED1_PIN, HIGH);
          digitalWrite(LED2_PIN, LOW);
        } else {
          digitalWrite(LED1_PIN, LOW);
          digitalWrite(LED2_PIN, HIGH);
        }
        myservo.write(hwTargetPos);
        hwTimer = millis();
        hwState = HW_AT_POS;
        break;
      case HW_AT_POS:
        if (millis() - hwTimer >= servoStopTime) {
          myservo.write(servoPosCenter);
          hwTimer = millis();
          hwState = HW_RETURN;
        }
        break;
      case HW_RETURN:
        if (millis() - hwTimer >= servoDelayTime) {
          digitalWrite(LED1_PIN, HIGH);
          digitalWrite(LED2_PIN, HIGH);
          hwState = HW_IDLE;
          // 鎖定已由上述檢測結果更新邏輯在物體離開時清除
        }
        break;
      default:
        break;
    }
  }
#endif
}

static size_t jpg_encode_stream(void * arg, size_t index, const void* data, size_t len){
    jpg_chunking_t *j = (jpg_chunking_t *)arg;
    if(!index){
        j->len = 0;
    }
    if(httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK){
        return 0;
    }
    j->len += len;
    return len;
}

//影像截圖
static esp_err_t capture_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;

    fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    size_t fb_len = 0;
    if(fb->format == PIXFORMAT_JPEG){
        fb_len = fb->len;
        res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    } else {
        jpg_chunking_t jchunk = {req, 0};
        res = frame2jpg_cb(fb, 80, jpg_encode_stream, &jchunk)?ESP_OK:ESP_FAIL;
        httpd_resp_send_chunk(req, NULL, 0);
        fb_len = jchunk.len;
    }
    esp_camera_fb_return(fb);
    return res;
}

//影像串流
static esp_err_t stream_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char part_buf[64];

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        return res;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    while(true){
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed");
            res = ESP_FAIL;
        } else {
          if(fb->format != PIXFORMAT_JPEG){
              bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
              esp_camera_fb_return(fb);
              fb = NULL;
              if(!jpeg_converted){
                  Serial.println("JPEG compression failed");
                  res = ESP_FAIL;
              }
          } else {
              _jpg_buf_len = fb->len;
              _jpg_buf = fb->buf;
          }
        }

        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }                
        if(fb){
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        } else if(_jpg_buf){
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if(res != ESP_OK){
            break;
        }
    }

    return res;
}

//指令參數控制
static esp_err_t cmd_handler(httpd_req_t *req){
    char*  buf;    //存取網址後帶的參數字串
    size_t buf_len;
    char variable[128] = {0,};  //存取參數var值
    char value[128] = {0,};     //存取參數val值
    String myCmd = "";

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char*)malloc(buf_len);
        if(!buf){
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
          if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) == ESP_OK &&
            httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK) {
          } 
          else {
            myCmd = String(buf);   //如果非官方格式不含var, val，則為自訂指令格式
          }
        }
        free(buf);
        buf = NULL;
    } else {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    Feedback="";Command="";cmd="";P1="";P2="";P3="";P4="";P5="";P6="";P7="";P8="";P9="";
    ReceiveState=0,cmdState=1,strState=1,questionstate=0,equalstate=0,semicolonstate=0;     
    if (myCmd.length()>0) {
      myCmd = "?"+myCmd;  //網址後帶的參數字串轉換成自訂指令格式
      for (int i=0;i<myCmd.length();i++) {
        getCommand(char(myCmd.charAt(i)));  //拆解自訂指令參數字串
      }
    }

    if (cmd.length()>0) {
      Serial.println("");
      //Serial.println("Command: "+Command);
      Serial.println("cmd= "+cmd+" ,P1= "+P1+" ,P2= "+P2+" ,P3= "+P3+" ,P4= "+P4+" ,P5= "+P5+" ,P6= "+P6+" ,P7= "+P7+" ,P8= "+P8+" ,P9= "+P9);
      Serial.println(""); 

      //自訂指令區塊  http://192.168.xxx.xxx/control?cmd=P1;P2;P3;P4;P5;P6;P7;P8;P9
      if (cmd=="your cmd") {
        // You can do anything
        // Feedback="<font color=\"red\">Hello World</font>";   //可為一般文字或HTML語法
      }
      else if (cmd=="ip") {  //查詢APIP, STAIP
        Feedback="AP IP: "+WiFi.softAPIP().toString();    
        Feedback+="<br>";
        Feedback+="STA IP: "+WiFi.localIP().toString();
      }  
      else if (cmd=="mac") {  //查詢MAC位址
        Feedback="STA MAC: "+WiFi.macAddress();
      }
#if defined(CAMERA_MODEL_AI_THINKER)        
      else if (cmd=="digitalwrite") {
        ledcDetach(P1.toInt());
        pinMode(P1.toInt(), OUTPUT);
        digitalWrite(P1.toInt(), P2.toInt());
      }   
      else if (cmd=="digitalread") {
        Feedback=String(digitalRead(P1.toInt()));
      }
      else if (cmd=="analogwrite") {   
        ledcAttach(P1.toInt(), 5000, 8);
        ledcWrite(P1.toInt(), P2.toInt());     
      }      
      else if (cmd=="analogread") {
        Feedback=String(analogRead(P1.toInt()));
      }
      else if (cmd=="touchread") {
        Feedback=String(touchRead(P1.toInt()));
      }
#endif      
#ifdef WIFI_AP_MODE
      else if (cmd=="resetwifi") {  //重設網路連線  
        for (int i=0;i<2;i++) {
          WiFi.begin(P1.c_str(), P2.c_str());
          Serial.print("Connecting to ");
          Serial.println(P1);
          long int StartTime=millis();
          while (WiFi.status() != WL_CONNECTED) {
              delay(500);
              if ((StartTime+5000) < millis()) break;
          } 
          Serial.println("");
          Serial.println("STAIP: "+WiFi.localIP().toString());
          Feedback="STAIP: "+WiFi.localIP().toString();
  
          if (WiFi.status() == WL_CONNECTED) {
            WiFi.softAP((WiFi.localIP().toString()+"_"+P1).c_str(), P2.c_str());
#if defined(CAMERA_MODEL_AI_THINKER)
            for (int j=0;j<2;j++) {    //若連不上WIFI設定閃光燈慢速閃爍
              ledcWrite(4,10);
              delay(300);
              ledcWrite(4,0);
              delay(300);    
            }
#endif            
            break;
          }
        }
      }
#endif
#if defined(CAMERA_MODEL_AI_THINKER)       
      else if (cmd=="flash") {  //控制內建閃光燈
        ledcAttach(4, 5000, 8);   
        int val = P1.toInt();
        ledcWrite(4,val);  
      }
      else if (cmd=="blink") {
        ledBlink = P1.toInt() == 1;
        if (!ledBlink) digitalWrite(4, LOW);
      }
#endif
      else if (cmd=="serial") { 
        if (P1!="" && P1!="stop") Serial.println(P1);
        if (P2!="" && P2!="stop") Serial.println(P2);
        Serial.println();
        // 轉發偵測結果給硬體狀態機
        if (hwStartupDone) {
            pendingClass = P1;
            pendingProb = P2.toFloat();
            pendingDetection = true;
        }
      }       
      else {
        Feedback="Command is not defined";
      }

      if (Feedback=="") Feedback=Command;  //若沒有設定回傳資料就回傳Command值
    
      const char *resp = Feedback.c_str();
      httpd_resp_set_type(req, "text/html");  //設定回傳資料格式
      httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");  //允許跨網域讀取
      return httpd_resp_send(req, resp, strlen(resp));
    } 
    else {
      //官方指令區塊，也可在此自訂指令  http://192.168.xxx.xxx/control?var=xxx&val=xxx
      int val = atoi(value);
      sensor_t * s = esp_camera_sensor_get();
      int res = 0;

      if(!strcmp(variable, "framesize")) {
        if(s->pixformat == PIXFORMAT_JPEG) 
          res = s->set_framesize(s, (framesize_t)val);
      }
      else if(!strcmp(variable, "quality")) res = s->set_quality(s, val);
      else if(!strcmp(variable, "contrast")) res = s->set_contrast(s, val);
      else if(!strcmp(variable, "brightness")) res = s->set_brightness(s, val);
      else if(!strcmp(variable, "hmirror")) res = s->set_hmirror(s, val);
      else if(!strcmp(variable, "vflip")) res = s->set_vflip(s, val);
#if defined(CAMERA_MODEL_AI_THINKER)
      else if(!strcmp(variable, "flash")) {
        ledcAttach(4, 5000, 8);        
        ledcWrite(4,val);
      } 
#endif     
      else {
          res = -1;
      }
  
      if(res){
          return httpd_resp_send_500(req);
      }

      if (buf) {
        Feedback = String(buf);
        const char *resp = Feedback.c_str();
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        return httpd_resp_send(req, resp, strlen(resp));  //回傳參數字串
      }
      else {
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        return httpd_resp_send(req, NULL, 0);
      }
    }
}

//顯示視訊參數狀態(須回傳json格式載入初始設定)
static esp_err_t status_handler(httpd_req_t *req){
    static char json_response[1024];

    sensor_t * s = esp_camera_sensor_get();
    char * p = json_response;
    *p++ = '{';   
    p+=sprintf(p, "\"flash\":%d,", 0);
    p+=sprintf(p, "\"framesize\":%u,", s->status.framesize);
    p+=sprintf(p, "\"quality\":%u,", s->status.quality);
    p+=sprintf(p, "\"brightness\":%d,", s->status.brightness);
    p+=sprintf(p, "\"contrast\":%d,", s->status.contrast);
    p+=sprintf(p, "\"hmirror\":%u,", s->status.hmirror); 
    p+=sprintf(p, "\"vflip\":%u", s->status.vflip);
    *p++ = '}';
    *p++ = 0;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}

//自訂網頁首頁
static const char PROGMEM INDEX_HTML[] = R"rawliteral(<!doctype html>
<html>
    <head>
        <meta charset="utf-8">
        <meta name="viewport" content="width=device-width,initial-scale=1">
        <meta http-equiv="Access-Control-Allow-Headers" content="Origin, X-Requested-With, Content-Type, Accept">
        <meta http-equiv="Access-Control-Allow-Methods" content="GET,POST,PUT,DELETE,OPTIONS">
        <meta http-equiv="Access-Control-Allow-Origin" content="*">
        <title>Teachable Machine</title>
        <style>
          body{font-family:Arial,Helvetica,sans-serif;background:#181818;color:#EFEFEF;font-size:16px}h2{font-size:18px}section.main{display:flex}#menu,section.main{flex-direction:column}#menu{display:flex;flex-wrap:nowrap;min-width:340px;background:#363636;padding:8px;border-radius:4px;margin-top:-10px;margin-right:10px}#content{display:flex;flex-wrap:wrap;align-items:stretch}figure{padding:0;margin:0;-webkit-margin-before:0;margin-block-start:0;-webkit-margin-after:0;margin-block-end:0;-webkit-margin-start:0;margin-inline-start:0;-webkit-margin-end:0;margin-inline-end:0}figure img{display:block;width:100%;height:auto;border-radius:4px;margin-top:8px}@media (min-width: 800px) and (orientation:landscape){#content{display:flex;flex-wrap:nowrap;align-items:stretch}figure img{display:block;max-width:100%;max-height:calc(100vh - 40px);width:auto;height:auto}figure{padding:0;margin:0;-webkit-margin-before:0;margin-block-start:0;-webkit-margin-after:0;margin-block-end:0;-webkit-margin-start:0;margin-inline-start:0;-webkit-margin-end:0;margin-inline-end:0}}section#buttons{display:flex;flex-wrap:nowrap;justify-content:space-between}#nav-toggle{cursor:pointer;display:block}#nav-toggle-cb{outline:0;opacity:0;width:0;height:0}#nav-toggle-cb:checked+#menu{display:none}.input-group{display:flex;flex-wrap:nowrap;line-height:22px;margin:5px 0}.input-group>label{display:inline-block;padding-right:10px;min-width:47%}.input-group input,.input-group select{flex-grow:1}.range-max,.range-min{display:inline-block;padding:0 5px}button{display:block;margin:5px;padding:0 12px;border:0;line-height:28px;cursor:pointer;color:#fff;background:#ff3034;border-radius:5px;font-size:16px;outline:0}button:hover{background:#ff494d}button:active{background:#f21c21}button.disabled{cursor:default;background:#a0a0a0}input[type=range]{-webkit-appearance:none;width:100%;height:22px;background:#363636;cursor:pointer;margin:0}input[type=range]:focus{outline:0}input[type=range]::-webkit-slider-runnable-track{width:100%;height:2px;cursor:pointer;background:#EFEFEF;border-radius:0;border:0 solid #EFEFEF}input[type=range]::-webkit-slider-thumb{border:1px solid rgba(0,0,30,0);height:22px;width:22px;border-radius:50px;background:#ff3034;cursor:pointer;-webkit-appearance:none;margin-top:-11.5px}input[type=range]:focus::-webkit-slider-runnable-track{background:#EFEFEF}input[type=range]::-moz-range-track{width:100%;height:2px;cursor:pointer;background:#EFEFEF;border-radius:0;border:0 solid #EFEFEF}input[type=range]::-moz-range-thumb{border:1px solid rgba(0,0,30,0);height:22px;width:22px;border-radius:50px;background:#ff3034;cursor:pointer}input[type=range]::-ms-track{width:100%;height:2px;cursor:pointer;background:0 0;border-color:transparent;color:transparent}input[type=range]::-ms-fill-lower{background:#EFEFEF;border:0 solid #EFEFEF;border-radius:0}input[type=range]::-ms-fill-upper{background:#EFEFEF;border:0 solid #EFEFEF;border-radius:0}input[type=range]::-ms-thumb{border:1px solid rgba(0,0,30,0);height:22px;width:22px;border-radius:50px;background:#ff3034;cursor:pointer;height:2px}input[type=range]:focus::-ms-fill-lower{background:#EFEFEF}input[type=range]:focus::-ms-fill-upper{background:#363636}.switch{display:block;position:relative;line-height:22px;font-size:16px;height:22px}.switch input{outline:0;opacity:0;width:0;height:0}.slider{width:50px;height:22px;border-radius:22px;cursor:pointer;background-color:grey}.slider,.slider:before{display:inline-block;transition:.4s}.slider:before{position:relative;content:"";border-radius:50%;height:16px;width:16px;left:4px;top:3px;background-color:#fff}input:checked+.slider{background-color:#ff3034}input:checked+.slider:before{-webkit-transform:translateX(26px);transform:translateX(26px)}select{border:1px solid #363636;font-size:14px;height:22px;outline:0;border-radius:5px}.image-container{position:relative;min-width:160px}.close{position:absolute;right:5px;top:5px;background:#ff3034;width:16px;height:16px;border-radius:100px;color:#fff;text-align:center;line-height:18px;cursor:pointer}.hidden{display:none}
        </style>
        <script src="https:\/\/ajax.googleapis.com/ajax/libs/jquery/1.8.0/jquery.min.js"></script>
        <script src="https:\/\/cdn.jsdelivr.net/npm/@tensorflow/tfjs@1.3.1/dist/tf.min.js"></script>
        <script src="https:\/\/cdn.jsdelivr.net/npm/@teachablemachine/image@0.8/dist/teachablemachine-image.min.js"></script>  
        <script src="https:\/\/cdn.jsdelivr.net/npm/@teachablemachine/pose@0.8/dist/teachablemachine-pose.min.js"></script>       
    </head>
    <body>
        <section class="main">
            <figure>
              <div id="stream-container" class="image-container">
                <img id="stream" src="" crossorigin="anonymous">
                <canvas id="canvas" width="0" height="0" style="display:none"></canvas>
              </div>
            </figure>
            <div id="result" style="color:red;font-size:18px;text-align:center;padding:10px;background:#222;border-radius:5px"></div>
            <div id="logo">
                <label for="nav-toggle-cb" id="nav-toggle">&#9776;&nbsp;&nbsp;Toggle settings</label>
            </div>
            <div id="content">
                <div id="sidebar">
                    <input type="checkbox" id="nav-toggle-cb">
                    <nav id="menu">
                        <div class="input-group" id="flash-group">
                            <label for="flash">Flash</label>
                            <div class="range-min">0</div>
                            <input type="range" id="flash" min="0" max="255" value="0" class="default-action">
                            <div class="range-max">255</div>
                        </div>                    
                        <div class="input-group" id="framesize-group">
                            <label for="framesize">Resolution</label>
                            <select id="framesize" class="default-action">
                                <option value="10">UXGA(1600x1200)</option>
                                <option value="9">SXGA(1280x1024)</option>
                                <option value="8">XGA(1024x768)</option>
                                <option value="7">SVGA(800x600)</option>
                                <option value="6">VGA(640x480)</option>
                                <option value="5">CIF(400x296)</option>
                                <option value="4">QVGA(320x240)</option>
                                <option value="3">HQVGA(240x176)</option>
                                <option value="0" selected="selected">QQVGA(160x120)</option>
                            </select>
                        </div>
                        <div class="input-group" id="quality-group">
                            <label for="quality">Quality</label>
                            <div class="range-min">10</div>
                            <input type="range" id="quality" min="10" max="63" value="10" class="default-action">
                            <div class="range-max">63</div>
                        </div>
                        <div class="input-group" id="brightness-group">
                            <label for="brightness">Brightness</label>
                            <div class="range-min">-2</div>
                            <input type="range" id="brightness" min="-2" max="2" value="0" class="default-action">
                            <div class="range-max">2</div>
                        </div>
                        <div class="input-group" id="contrast-group">
                            <label for="contrast">Contrast</label>
                            <div class="range-min">-2</div>
                            <input type="range" id="contrast" min="-2" max="2" value="0" class="default-action">
                            <div class="range-max">2</div>
                        </div>
                        <div class="input-group" id="hmirror-group">
                            <label for="hmirror">H-Mirror</label>
                            <div class="switch">
                                <input id="hmirror" type="checkbox" class="default-action" checked="checked">
                                <label class="slider" for="hmirror"></label>
                            </div>
                        </div>
                        <div class="input-group" id="vflip-group">
                            <label for="vflip">V-Flip</label>
                            <div class="switch">
                                <input id="vflip" type="checkbox" class="default-action" checked="checked">
                                <label class="slider" for="vflip"></label>
                            </div>
                        </div>
                    </nav>
                </div>
            </div>
            <div id="debug" style="font-size:12px;color:#888;margin-top:8px;max-height:150px;overflow-y:auto;background:#1a1a1a;padding:6px;border-radius:4px"></div>
        </section>

        <script>
        var baseHost = document.location.origin;
        const MODEL_URL = "https://jackylung.github.io/ESP32Cam_Teachable_Machine/tm-model/";
        const CACHE_BUST = "?v=" + Date.now();

        var log = function(msg) {
            console.log(msg);
            var el = document.getElementById('debug');
            if (!el) return;
            el.innerHTML += msg + "<br>";
            var lines = el.innerHTML.split("<br>");
            if (lines.length > 50) {
                el.innerHTML = lines.slice(lines.length - 50).join("<br>");
            }
            el.scrollTop = el.scrollHeight;
        }
        const MODEL_KIND = "image";
        let Model = null;
        let maxPredictions = 0;

        document.addEventListener('DOMContentLoaded', function (event) {
            log("DOMContentLoaded fired");
            log("baseHost: " + baseHost);
            log("MODEL_URL: " + MODEL_URL + CACHE_BUST);
            const hide = el => el.classList.add('hidden');

            const updateValue = (el, value, updateRemote) => {
              updateRemote = updateRemote == null ? true : updateRemote;
              let initialValue;
              if (el.type === 'checkbox') {
                initialValue = el.checked;
                value = !!value;
                el.checked = value;
              } else {
                initialValue = el.value;
                el.value = value;
              }
              if (updateRemote && initialValue !== value) updateConfig(el);
            };

            var updateConfig = function (el) {
              let value;
              switch (el.type) {
                case 'checkbox': value = el.checked ? 1 : 0; break;
                case 'range': case 'select-one': value = el.value; break;
                case 'button': case 'submit': value = '1'; break;
                default: return;
              }
              fetch(`${baseHost}/control?var=${el.id}&val=${value}`);
            }

            document.querySelectorAll('.close').forEach(el => {
              el.onclick = () => hide(el.parentNode);
            });

            fetch(`${baseHost}/status`)
              .then(res => { log("status fetch: " + res.status); return res.json(); })
              .then(state => {
                document.querySelectorAll('.default-action').forEach(el => {
                  updateValue(el, state[el.id], false);
                });
                log("settings loaded from ESP32");
              })
              .catch(e => log("status fetch error: " + e.message));

            const view = document.getElementById('stream');

            document.querySelectorAll('.default-action').forEach(el => {
              el.onchange = () => updateConfig(el);
            });

            // Auto-load model with LED blink
            log("starting loadModel...");
            loadModel();
        });

        var loadModel = async () => {
            var result = document.getElementById('result');
            log("loadModel: starting blink");
            fetch(baseHost + '/control?cmd=blink;1');
            result.innerHTML = "Loading model...";
            log("loadModel: fetching from " + MODEL_URL + "model.json" + CACHE_BUST);
            try {
                var t0 = Date.now();
                if (MODEL_KIND == "image") {
                    log("loadModel: loading tmImage...");
                    Model = await tmImage.load(MODEL_URL + "model.json" + CACHE_BUST, MODEL_URL + "metadata.json" + CACHE_BUST);
                } else {
                    log("loadModel: loading tmPose...");
                    Model = await tmPose.load(MODEL_URL + "model.json" + CACHE_BUST, MODEL_URL + "metadata.json" + CACHE_BUST);
                }
                var t1 = Date.now();
                maxPredictions = Model.getTotalClasses();
                log("loadModel: loaded in " + (t1-t0) + "ms, classes: " + maxPredictions);
                result.innerHTML = "";
                log("loadModel: stopping blink, starting predict loop");
                fetch(baseHost + '/control?cmd=blink;0');
                setTimeout(predictLoop, predictInterval);
            } catch(e) {
                log("loadModel ERROR: " + e.message);
                result.innerHTML = "Model load failed: " + e.message;
                fetch(baseHost + '/control?cmd=blink;0');
            }
        }

        var predictLoop = async () => {
            if (!Model) return;
            var view = document.getElementById('stream');
            var canvas = document.getElementById('canvas');
            var context = canvas.getContext('2d');
            var result = document.getElementById('result');

            try {
                var resp = await fetch(baseHost + '/capture?t=' + Date.now());
                var blob = await resp.blob();
                var url = URL.createObjectURL(blob);
                view.onload = async function() {
                    var w = view.naturalWidth || view.width;
                    var h = view.naturalHeight || view.height;
                    if (w === 0 || h === 0) {
                        log("predict: image has zero dimensions, skipping");
                        setTimeout(predictLoop, predictInterval);
                        return;
                    }
                    canvas.width = w;
                    canvas.height = h;
                    context.drawImage(view, 0, 0, w, h);
                    URL.revokeObjectURL(url);

                    try {
                        var prediction;
                        if (MODEL_KIND == "image") {
                            prediction = await Model.predict(canvas);
                        } else {
                            var r = await Model.estimatePose(canvas);
                            prediction = await Model.predict(r.posenetOutput);
                        }

                        var data = "";
                        var maxClass = "", maxProb = 0;
                        for (let i = 0; i < maxPredictions; i++) {
                            if (prediction[i].probability > maxProb) {
                                maxClass = prediction[i].className;
                                maxProb = prediction[i].probability;
                            }
                            data += prediction[i].className + "," + prediction[i].probability.toFixed(2) + "<br>";
                        }
                        result.innerHTML = "<strong>" + maxClass + "</strong> " + (maxProb*100).toFixed(1) + "%<br><br>" + data;
                        log("predict: " + maxClass + " " + maxProb.toFixed(4));

                        fetch(baseHost + '/control?serial=' + maxClass + ';' + maxProb + ';stop');
                    } catch(e) {
                        log("predict error: " + e.message);
                    }
                    setTimeout(predictLoop, predictInterval);
                };
                view.src = url;
            } catch(e) {
                log("capture error: " + e.message);
                setTimeout(predictLoop, predictInterval);
            }
        }
        </script>
    </body>
</html>)rawliteral";

//網頁首頁   http://192.168.xxx.xxx
static esp_err_t index_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "<script>var predictInterval=%d;</script>\n", predictInterval);
    httpd_resp_send_chunk(req, buf, n);
    httpd_resp_send_chunk(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
    return httpd_resp_send_chunk(req, NULL, 0);
}

//自訂網址路徑要執行的函式
void startCameraServer(){
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();  //可在HTTPD_DEFAULT_CONFIG()中設定Server Port 

  //http://192.168.xxx.xxx/
  httpd_uri_t index_uri = {
      .uri       = "/",
      .method    = HTTP_GET,
      .handler   = index_handler,
      .user_ctx  = NULL
  };

  //http://192.168.xxx.xxx/status
  httpd_uri_t status_uri = {
      .uri       = "/status",
      .method    = HTTP_GET,
      .handler   = status_handler,
      .user_ctx  = NULL
  };

  //http://192.168.xxx.xxx/control
  httpd_uri_t cmd_uri = {
      .uri       = "/control",
      .method    = HTTP_GET,
      .handler   = cmd_handler,
      .user_ctx  = NULL
  }; 

  //http://192.168.xxx.xxx/capture
  httpd_uri_t capture_uri = {
      .uri       = "/capture",
      .method    = HTTP_GET,
      .handler   = capture_handler,
      .user_ctx  = NULL
  };

  //http://192.168.xxx.xxx:81/stream
  httpd_uri_t stream_uri = {
      .uri       = "/stream",
      .method    = HTTP_GET,
      .handler   = stream_handler,
      .user_ctx  = NULL
  };
  
  Serial.printf("Starting web server on port: '%d'\n", config.server_port);  //Server Port
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
      //註冊自訂網址路徑對應執行的函式
      httpd_register_uri_handler(camera_httpd, &index_uri);
      httpd_register_uri_handler(camera_httpd, &cmd_uri);
      httpd_register_uri_handler(camera_httpd, &status_uri);
      httpd_register_uri_handler(camera_httpd, &capture_uri);
  }
  
  config.server_port += 1;  //Stream Port
  config.ctrl_port += 1;    //UDP Port
  Serial.printf("Starting stream server on port: '%d'\n", config.server_port);
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
      httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}

//自訂指令拆解參數字串置入變數
void getCommand(char c)
{
  if (c=='?') ReceiveState=1;
  if ((c==' ')||(c=='\r')||(c=='\n')) ReceiveState=0;
  
  if (ReceiveState==1)
  {
    Command=Command+String(c);
    
    if (c=='=') cmdState=0;
    if (c==';') strState++;
  
    if ((cmdState==1)&&((c!='?')||(questionstate==1))) cmd=cmd+String(c);
    if ((cmdState==0)&&(strState==1)&&((c!='=')||(equalstate==1))) P1=P1+String(c);
    if ((cmdState==0)&&(strState==2)&&(c!=';')) P2=P2+String(c);
    if ((cmdState==0)&&(strState==3)&&(c!=';')) P3=P3+String(c);
    if ((cmdState==0)&&(strState==4)&&(c!=';')) P4=P4+String(c);
    if ((cmdState==0)&&(strState==5)&&(c!=';')) P5=P5+String(c);
    if ((cmdState==0)&&(strState==6)&&(c!=';')) P6=P6+String(c);
    if ((cmdState==0)&&(strState==7)&&(c!=';')) P7=P7+String(c);
    if ((cmdState==0)&&(strState==8)&&(c!=';')) P8=P8+String(c);
    if ((cmdState==0)&&(strState>=9)&&((c!=';')||(semicolonstate==1))) P9=P9+String(c);
    
    if (c=='?') questionstate=1;
    if (c=='=') equalstate=1;
    if ((strState>=9)&&(c==';')) semicolonstate=1;
  }
}
