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
ESP32-CAM 以 **AP 模式（Wi-Fi 基地台）** 運作，手機直接連接 ESP32 的 AP，不需要額外路由器或熱點。

檔案 `esp32cam_TM.ino` 中可修改 AP 名稱與密碼：
```cpp
const char* apssid = "KTS_SmartBin_AP";   // AP 名稱
const char* appassword = "12345678";       // AP 密碼（至少 8 字元）
```

- **Web Server**：`http://192.168.4.1`（埠 80）
- **Stream Server**：`http://192.168.4.1:81/stream`（埠 81）

> 說明：為何不使用 STA 連線手機熱點？
> ESP32 只有單一 WiFi 無線電，在 AP+STA 雙模式下 STA 掃描連線會干擾 AP 封包轉發，導致手機 HTTP 連線不穩定。純 AP 模式最穩定可靠。

## 使用流程
1. ESP32-CAM 接電 → 自動啟動 AP
2. 手機連接 ESP32 的 Wi-Fi（如 `KTS_SmartBin_AP`）
3. 開啟瀏覽器輸入 `http://192.168.4.1`
4. 頁面自動載入 TM 模型並開始辨識
5. GPIO4 LED：模型載入中慢閃 → 載入完成熄滅

## 頁面配置
- **偵測結果**：顯示在影像下方、Restart 按鈕上方
- **Restart 按鈕**：點擊後顯示 "Restarting..."，自動輪詢 `/status` 等待 ESP32 重啟完成，偵測到回應後自動重新載入頁面（不再停留在黑色畫面）
- **設定選單**：點擊左上角 ☰ 展開/收起

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
6. 重新整理 ESP32-CAM 的瀏覽器頁面即可使用新模型（已加入 cache-busting 參數，跳過瀏覽器快取）

> JS 會從 GitHub Pages 載入模型：`https://jackylung.github.io/ESP32Cam_Teachable_Machine/tm-model/`
> 此 Repository 必須設為 **Public**，GitHub Pages 才能正常運作。

## 注意事項
- 使用 ESP32 Arduino Core 3.x，LEDC API 為 `ledcAttach(pin, freq, resolution)` 新版語法
- 如需支援 Core 2.x，需將 `ledcAttach` 改回 `ledcAttachPin` + `ledcSetup`
- 不支援 LCD 顯示（已移除相關程式碼）
