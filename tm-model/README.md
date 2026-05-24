# Teachable Machine Model

此目錄存放 Teachable Machine 訓練好的模型，透過 GitHub Pages 託管給 ESP32-CAM 使用。

## 如何更新模型

### 1. 在 Teachable Machine 訓練模型
前往 https://teachablemachine.withgoogle.com/ 訓練你的影像分類或姿勢辨識模型。

### 2. 匯出模型
- 點選「Export Model」→「Download my model」
- 解壓縮後會得到以下檔案：
  - `model.json`
  - `metadata.json`
  - `weights.bin`（或一群 `weights_*.bin`）

### 3. 覆蓋此目錄的檔案
將上述檔案複製到本目錄（`tm-model/`），取代舊檔案。

### 4. 上傳到 GitHub
```bash
git add tm-model/
git commit -m "update TM model"
git push
```

### 5. 確認模型可正常存取
等待 GitHub Pages 部署（約 1-2 分鐘），然後開啟瀏覽器測試：
```
https://jackylung.github.io/ESP32Cam_Teachable_Machine/tm-model/model.json
```
看到 JSON 內容即代表部署成功。

### 6. 重新整理 ESP32-CAM 網頁
不需重燒韌體。只要重新整理瀏覽器頁面，JS 就會從 GitHub 載入最新模型。

---

## 注意事項
- 請勿刪除或重新命名 `model.json`、`metadata.json`，否則 JS 無法載入
- 如模型結構改變（不同分類數量），瀏覽器會自動適應新模型
- 若瀏覽器快取舊模型，可按 `Ctrl+F5` 強制重新整理
