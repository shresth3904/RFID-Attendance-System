# 📟 ESP32 Offline-First RFID Attendance System

An enterprise-grade, highly optimized RFID attendance system built on the ESP32. This project is designed to operate completely offline, logging attendance data to local flash memory, and automatically syncing with a Python (Flask) backend when a known WiFi network becomes available.

Unlike standard ESP32 projects that crash when loading large JSON databases into limited RAM, this system features a custom **O(1) memory-footprint architecture**. It uses line-by-line CSV streaming and HTTP Chunked Transfer Encoding to support **thousands of registered users** and **tens of thousands of attendance logs** without memory fragmentation or stack overflows.

## ✨ Key Features
* **Offline-First Logging:** Attendance is recorded locally with precise timestamps using a DS3231 Hardware RTC.
* **Low-RAM Architecture:** Streams CSV data directly from LittleFS to the web client, bypassing RAM limits. 
* **AP Mode Web Dashboard:** Hosts a local web server (Access Point) to manage the device without needing internet.
* **Auto-Sync Engine:** Automatically detects configured WiFi networks and securely POSTs pending CSV logs to the server.
* **Full CRUD Interface:** Add users, delete users, and download CSVs directly from the ESP32's web dashboard.
* **Real-Time Feedback:** Includes an SSD1306 OLED display, dual LEDs (Red/Green), and a buzzer for instant scan feedback.
* **Configurable Security:** Changeable AP passwords and secure SSL/TLS backend communication.

---

## 🛠️ Hardware Requirements
* **Microcontroller:** ESP32 (e.g., NodeMCU-32S)
* **RFID Reader:** MFRC522 (13.56MHz)
* **RTC Module:** DS3231 (for accurate offline timekeeping)
* **Display:** 0.96" SSD1306 OLED (I2C)
* **Indicators:** 2x LEDs (Red & Green), 1x Active Buzzer
* **Misc:** Jumper wires, breadboard, 330Ω resistors for LEDs.

### 🔌 Pin Configuration
| Component | ESP32 Pin | Note |
| :--- | :--- | :--- |
| **MFRC522** (RST) | GPIO 4 | Reset |
| **MFRC522** (SDA/SS) | GPIO 5 | SPI Chip Select |
| **MFRC522** (SCK) | GPIO 18 | SPI Clock |
| **MFRC522** (MISO) | GPIO 19 | SPI MISO |
| **MFRC522** (MOSI) | GPIO 23 | SPI MOSI |
| **OLED & RTC** (SDA) | GPIO 21 | I2C Data |
| **OLED & RTC** (SCL) | GPIO 22 | I2C Clock |
| **Green LED** | GPIO 15 | Success Indicator |
| **Red LED** | GPIO 2 | Error Indicator |
| **Buzzer** | GPIO 27 | Audio Feedback |

---

## 💻 Software Stack
* **C++ / Arduino IDE:** Core firmware logic.
* **LittleFS:** Efficient local file system for ESP32 flash memory.
* **ArduinoJson:** Used strictly for lightweight configuration files (`wifi.json`, `sync.json`).
* **Python / Flask & SQLite:** (Backend) Receives, parses, and permanently stores the uploaded `.csv` files.

---

## 🚀 Installation & Setup

### 1. Flash the ESP32
1. Install the **ESP32 Board Package** in the Arduino IDE.
2. Install the following libraries via the Library Manager:
   * `MFRC522` by GithubCommunity
   * `Adafruit SSD1306` & `Adafruit GFX Library`
   * `RTClib` by Adafruit
   * `ArduinoJson` by Benoit Blanchon
3. Select `LittleFS` as the flash partition scheme in the Arduino IDE (e.g., 1.5MB APP / 1.5MB LittleFS).
4. Compile and upload the code to your ESP32.

### 2. Initial Configuration (AP Mode)
1. Once powered on, the ESP32 will host its own WiFi network.
2. Connect your phone or laptop to the network:
   * **SSID:** `ESP32-RFID`
   * **Password:** `12345678` (Default)
3. Open a web browser and navigate to the IP address displayed on the OLED screen (usually `http://192.168.4.1`).
4. **Set the RTC Time:** Navigate to the "Set Date & Time" page to sync the hardware clock.
5. **Add Users:** Go to "Add User", scan an RFID card, and type in the student/employee details.

### 3. Connecting to the Backend
1. On the ESP32 Dashboard, click **Configure WiFi**.
2. Enter your local WiFi SSID and Password.
3. The ESP32 will reboot, connect to your router, and automatically begin syncing pending CSV files to your designated Flask server endpoint (`/upload`).

---

## 📂 File System Architecture (LittleFS)
To prevent heap fragmentation, this system relies heavily on local files rather than RAM variables:
* `/users.csv` - The master database of registered users. Read line-by-line during scans.
* `/YYYY-MM-DD_Attendance.csv` - Daily log files created dynamically.
* `/wifi.json` - Stores user-configured network credentials.
* `/ap_config.json` - Stores the custom admin dashboard password.
* `/sync.json` - Tracks which daily logs have successfully reached the server.

---

## 🔮 Future Improvements
- [ ] Implement a sleep mode for the OLED display to prevent pixel burn-in.
- [ ] Add a hardware Watchdog Timer (WDT) to automatically recover from I2C/SPI hangs.
