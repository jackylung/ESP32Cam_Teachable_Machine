# ESP32Cam Teachable Machine

ESP32-CAM (AI Thinker) 結合 TensorFlow.js Teachable Machine 的即時影像辨識系統。

## 硬體需求
- ESP32-CAM 開發板（AI Thinker 型号，具備 PSRAM）
- FTDI 燒錄器（3.3V）
- 手機 / 平板 / 筆電（Wi-Fi 用戶端）

## 專案結構
```
ESP32Cam_Teachable_Machine/
├── AGENTS.md                  # 本專案說明文件
├── .gitignore                 # 忽略備份目錄 (*副本*/、references/)
├── esp32cam_TM/
│   ├── esp32cam_TM.ino        # 主程式（Arduino IDE 專案）
│   └── camera_pins.h          # 各型號相機腳位定義
├── tm-model/
│   ├── README.md              # 模型上傳說明
│   ├── model.json             # TM 模型定義（由 TM 匯出）
│   ├── metadata.json          # 模型元資料（由 TM 匯出）
│   └── weights.bin            # 模型權重（由 TM 匯出）
└── CameraWebServer_example/   # ESP32 官方範例（參考用）
```

## Arduino IDE 設定
- 開發板：`AI Thinker ESP32-CAM`
- 需安裝 ESP32 套件（espressif/arduino-esp32 v3.x）
- Flash Size：`4MB (32Mb)`
- Partition Scheme：`Huge APP (3MB No OTA/1MB SPIFFS)`
- Upload Speed：`115200`

## 硬體接線（燒錄模式）
| FTDI | ESP32-CAM |
|------|-----------|
| 3.3V | VCC       |
| GND  | GND       |
| TX   | UOR (GPIO3) |
| RX   | UOT (GPIO1) |
| GND  | IO0 (燒錄時接地) |

燒錄步驟：
1. IO0 接 GND
2. 插入 USB
3. 點選 Arduino IDE 上傳
4. 上傳完成後拔掉 IO0 跳線
5. 按 RST 重啟

## 硬體接線（外部控制）
| ESP32-CAM | 元件 |
|-----------|------|
| GPIO4     | 內建閃光燈（PWM 0~255） |
| GPIO12    | LED1（PET 偵測時點亮） |
| GPIO13    | LED2（CAN 偵測時點亮） |
| GPIO14    | 舵機信號線 |

## 控制邏輯
1. **開機校準**：舵機移至中心 → 最大角度 → 回中心，確認運作正常
2. **待機狀態**：LED1 + LED2 同時亮起，舵機停在中心位置，等待辨識
3. **PET 偵測**（信心度 > 70%）：LED1 亮、LED2 滅，舵機轉至 `servoPosMax`（125°），停留 1 秒後回中心，LED1+LED2 重新亮起
4. **CAN 偵測**（信心度 > 70%）：LED2 亮、LED1 滅，舵機轉至 `servoPosMin`（55°），停留 1 秒後回中心，LED1+LED2 重新亮起

所有控制參數統一寫在程式開頭（腳位定義、舵機角度、門檻值），可在 `#define` 與 `const int` 區塊集中修改。

## Wi-Fi 設定
ESP32-CAM 支援兩種連線模式，**二選一**，在 `esp32cam_TM.ino` 開頭註解/取消註解對應的 `#define` 來切換：

```cpp
#define WIFI_AP_MODE    // ESP32 作為基地台，手機直接連接
// #define WIFI_STA_MODE // ESP32 連線到現有 WiFi 路由器
```

### AP 模式（預設）
手機直接連接 ESP32 的 AP，不需要額外路由器或熱點。
```cpp
const char* apssid = "KTS_SmartBin_AP";
const char* appassword = "12345678";
```
- **Web Server**：`http://192.168.4.1`

### STA 模式
ESP32 連線到現有 WiFi 路由器，手機與 ESP32 在同一區域網路即可存取。
```cpp
const char* sta_ssid = "your_wifi_ssid";
const char* sta_password = "your_wifi_password";
```
- 請透過 Serial Monitor 查看 DHCP 指派的 IP 位址
- **Web Server**：`http://<DHCP_IP>`

> 注意：**兩種模式不可同時啟用**。AP+STA 雙模式會因單一無線電分時切換導致 AP 不穩定，故不支援。

## 使用流程
### AP 模式
1. ESP32-CAM 接電 → 自動啟動 AP
2. 手機連接 ESP32 的 Wi-Fi（如 `KTS_SmartBin_AP`）（手機須開啟行動數據以下載 TM 模型）
3. 開啟瀏覽器輸入 `http://192.168.4.1`
4. 頁面自動載入 TM 模型並開始辨識（每 1 秒擷取一張 `/capture` 靜態影像進行預測）
5. GPIO4 LED：模型載入中慢閃 → 載入完成熄滅

### STA 模式
1. ESP32-CAM 接電 → 自動連線 WiFi 路由器
2. 透過 Serial Monitor 查看 ESP32 取得的 IP 位址
3. 手機連線到同一個 WiFi 路由器
4. 開啟瀏覽器輸入 ESP32 的 IP 位址

> 預測流程：瀏覽器每 `predictInterval` ms 透過 HTTP GET 向 `/capture` 索取一張 JPEG 靜態影像，再餵入 TM 模型進行辨識。此方式取代傳統 MJPEG 串流（已移除），大幅降低 Wi-Fi 頻寬負載，讓 1fps 預測更加穩定。

## 頁面配置
- **偵測結果**：顯示在影像下方、設定選單上方
- **日誌**：顯示在設定選單下方

## 設定頁面功能
| 設定 | 說明 |
|------|------|
| Flash | 內建閃光燈亮度 (0~255) |
| Resolution | 影像解析度（預設 QQVGA 160×120，降低頻寬提升穩定度） |
| Quality | JPEG 壓縮品質 (10~63) |
| Brightness | 亮度 (-2~2) |
| Contrast | 對比度 (-2~2) |
| H-Mirror | 水平鏡像 |
| V-Flip | 垂直翻轉 |

## 預測設定（在程式開頭 `const int` 區塊修改）
| 參數 | 預設值 | 說明 |
|------|--------|------|
| `predictInterval` | 1000 | 預測間隔（ms），每次向 `/capture` 索取單張 JPEG |
| `acceptanceRate` | 0.7 | 觸發舵機/LED 的信心度門檻 |
| `servoStopTime` | 1000 | 舵機抵達定位後停留時間（ms） |
| `servoDelayTime` | 1000 | 舵機回中心後等待時間（ms） |

## 自訂 HTTP API
```
http://<IP>/control?cmd=<command>;P1;P2;...;P9
http://<IP>/control?var=<variable>&val=<value>
http://<IP>/capture         # 取得單張影像
http://<IP>/status           # 取得目前設定值 (JSON)
http://<IP>/control?ip       # 取得 AP/STA IP
http://<IP>/control?flash=val # 控制閃光燈
http://<IP>/control?blink=1  # 啟動 LED 慢閃
http://<IP>/control?blink=0  # 關閉 LED 慢閃
http://<IP>/control?serial=<class>;<confidence>;stop  # 傳遞 TM 辨識結果
http://<IP>/control?resetwifi=ssid;password  # 重設 WiFi 連線（僅 AP 模式）
```

## 更新 TM 模型（不需重燒韌體）
1. 在 https://teachablemachine.withgoogle.com/ 訓練新模型
2. 點選「Export Model」→「Download my model」
3. 解壓縮後將 `model.json`、`metadata.json`、`*.bin` 複製到 `tm-model/` 目錄
4. Git commit & push 到 GitHub
5. 等待 GitHub Pages 部署完成（約 1-2 分鐘）
6. 重新整理 ESP32-CAM 的瀏覽器頁面即可使用新模型（已加入 cache-busting 參數，跳過瀏覽器快取）

> JS 會從 GitHub Pages 載入模型：`https://jackylung.github.io/ESP32Cam_Teachable_Machine/tm-model/`
> 此 Repository 必須設為 **Public**，GitHub Pages 才能正常運作。

## 注意事項
- 使用 ESP32 Arduino Core 3.x，LEDC API 為 `ledcAttach(pin, freq, resolution)` 新版語法
- 如需支援 Core 2.x，需將 `ledcAttach` 改回 `ledcAttachPin` + `ledcSetup`
- 舵機控制使用 Arduino `ESP32Servo.h` 函式庫（需在 Arduino 程式庫管理員安裝 ESP32Servo）
- 不支援 LCD 顯示（已移除相關程式碼）
