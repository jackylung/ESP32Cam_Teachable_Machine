
請按照以下要求控制硬件：
#include <ESP32Servo.h> //Include the ESP32Servo library to control the servo motor.
// Pin definitions
#define LED1_PIN 12 //Turn on when PET is detected.
#define LED2_PIN 13 //Turn on when CAN is detected.
#define SERVO_PIN 14  //When PET is detected, servo moves to the servoPosMax. When CAN is detected, servo moves to the servoPosMin.
int LED1_status = 0;
int LED2_status = 0;
int detectionStatus = 0; //0: No detection, 1: PET, 2: CAN
float acceptanceRate = 0.7; //Acceptance rate for PET and CAN detection to trigger the servo.

// Create servo object
Servo myservo;
const int servoPosCenter = 90; //Center position of the servo.
const int servoPosMax = servoPosCenter + 35;
const int servoPosMin = servoPosCenter - 35;
const int servoDelayTime = 1000; //Delay time for servo to move to the center position.
const int servoStopTime = 500; //Delay time for servo to stop at the center, max, or min position.

控制邏輯：
1. 啟動時，將 servo 移到servoPosCenter，再移到servoPosMax 和 servoPosMax，之間停頓的時間是servoStopTime，讓用戶判斷舵機是否運作正常。
2. 在待機狀態下，保持LED1_PIN 和 LED2_PIN LED 開啟，並且保持 servo 移到 servoPosCenter位置，這個時候才能開始辨識PET或CAN，對比acceptanceRate，決定是否要移動servo。
3. 當有PET或CAN被辨識時並且準確率 > acceptanceRate，將對應的LED1_PIN或LED2_PIN LED開啟 而另外一個LED關閉，然後將 servo 移到 對應的servoPosMin 或 servoPosMax位置，然後停頓servoStopTime秒，再移回 servoPosCenter等待下一個辨識結果。當servo移回servoPosCenter時，LED1_PIN 和 LED2_PIN LED 保持打開示意用戶可以放入新的PET或CAN供檢測。

所有變量和設置的參數需要統一寫在程式開頭設置區域方便統一管理。如有不清楚的地方請待我確認。

