# ESP32Cam Teachable Machine

ESP32-CAM (AI Thinker) 結合 TensorFlow.js Teachable Machine 的即時影像辨識系統。

## 硬體需求
- ESP32-CAM 開發板（AI Thinker 型号，具備 PSRAM）
- FTDI 燒錄器（3.3V）
- 手機熱點（Wi-Fi）

## 專案結構
```
ESP32Cam_Teachable_Machine/
├── AGENTS.md                  # 本專案說明文件
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

## Wi-Fi 設定
檔案 `esp32cam_TM.ino` 中修改：
```cpp
const char* ssid     = "你的手機熱點名稱";
const char* password = "你的手機熱點密碼";
```

ESP32-CAM 開機後會自動連線手機熱點，連線成功後 Server 啟動：
- **Web Server**：`http://<ESP32_IP>`（埠 80）
- **Stream Server**：`http://<ESP32_IP>:81/stream`（埠 81）

## 使用流程
1. 手機開啟熱點（SSID/密碼需與程式一致）
2. ESP32-CAM 接電 → 自動連線熱點
3. 開啟瀏覽器輸入 ESP32 的 IP 位址
4. 頁面自動載入 TM 模型並開始辨識
5. GPIO4 LED：模型載入中慢閃 → 載入完成熄滅

## 設定頁面功能
| 設定 | 說明 |
|------|------|
| Flash | 內建閃光燈亮度 (0~255) |
| Resolution | 影像解析度 |
| Quality | JPEG 壓縮品質 (10~63) |
| Brightness | 亮度 (-2~2) |
| Contrast | 對比度 (-2~2) |
| H-Mirror | 水平鏡像 |
| V-Flip | 垂直翻轉 |

## 自訂 HTTP API
```
http://<IP>/control?cmd=<command>;P1;P2;...;P9
http://<IP>/control?var=<variable>&val=<value>
http://<IP>/capture         # 取得單張影像
http://<IP>/status           # 取得目前設定值 (JSON)
http://<IP>/control?restart  # 重啟 ESP32
http://<IP>/control?ip       # 取得 AP/STA IP
http://<IP>/control?flash=val # 控制閃光燈
http://<IP>/control?blink=1  # 啟動 LED 慢閃
http://<IP>/control?blink=0  # 關閉 LED 慢閃
```

## 更新 TM 模型（不需重燒韌體）
1. 在 https://teachablemachine.withgoogle.com/ 訓練新模型
2. 點選「Export Model」→「Download my model」
3. 解壓縮後將 `model.json`、`metadata.json`、`*.bin` 複製到 `tm-model/` 目錄
4. Git commit & push 到 GitHub
5. 等待 GitHub Pages 部署完成（約 1-2 分鐘）
6. 重新整理 ESP32-CAM 的瀏覽器頁面即可使用新模型

> JS 會從 GitHub Pages 載入模型：`https://jackylung.github.io/ESP32Cam_Teachable_Machine/tm-model/`

## 注意事項
- 使用 ESP32 Arduino Core 3.x，LEDC API 為 `ledcAttach(pin, freq, resolution)` 新版語法
- 如需支援 Core 2.x，需將 `ledcAttach` 改回 `ledcAttachPin` + `ledcSetup`
- 不支援 LCD 顯示（已移除相關程式碼）
