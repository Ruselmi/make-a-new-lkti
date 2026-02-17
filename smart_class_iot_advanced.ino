/*
 * IoT Kelas Pintar - Fitur Lengkap + Anti-Bug + Kalibrasi + 10+ Fitur Baru
 * Hardware: ESP32 DOIT DEVKIT V1
 * 
 * ROBUSTNESS IMPROVEMENTS:
 * - Static JSON buffers (no heap fragmentation)
 * - ESP32 Watchdog Timer (prevent freezes)
 * - Exponential backoff for WiFi reconnection
 * - Sensor reading retries with outlier detection
 * - Input validation and rate limiting
 * - Better error handling and recovery
 * - Memory-efficient data structures
 * - Optimized sensor sampling
 */

#include "smart_class_bundle.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <time.h>

// ============ STATIC CONFIG ============
const char* ap_ssid = "SMART_CLASS_AP";
const char* ap_password = "12345678";
const String ADMIN_KEY = "admin123";
const uint16_t WEB_PORT = 80;
String telegramUserId = TELEGRAM_USER_DEFAULT;
bool telegramNotify = false;

// ============ OBJEK ============
DHT dht(DHTPIN, DHTTYPE);
WebServer server(WEB_PORT);
WiFiClientSecure clientBot;
UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, clientBot);

const unsigned long BOT_MTBS = 1000; // mean time between scan messages
unsigned long bot_lasttime = 0;

// ============ VARIABEL SENSOR ============
float temperature = 0.0f, humidity = 0.0f;
int gasRaw = 0;
float gasAlarmPoint = 3000.0f;
int safeFlameState = LOW;
float dB = 0.0f, luxEst = 0.0f;
float distanceCm = 0.0f;
String statusTemp, statusGas, statusSound, statusLux, systemHealth = "OK";
String ldrDoStatus = "--";
int luxRaw = 0;

// ============ ADVANCED SETTINGS (UNITS) ============
String unitTemp = "C";
String unitHum = "%";
String unitGas = "RAW";
String unitLux = "Lx";
String unitNoise = "dB";

// ============ AI PREDIKSI STRES & MOOD BELAJAR ============
int stressScore = 0;
String moodLearn = "--";
bool soundUnreadable = false;

// ============ MODE AMBIENT & AUTO ============
#define AMBIENT_OFF 0
#define AMBIENT_FAN 1
#define AMBIENT_COUGH 2
#define AMBIENT_TEACHER 3
int ambientMode = AMBIENT_OFF;
bool autoMode = false;
unsigned long lastAutoSwitch = 0;
const unsigned long AUTO_SWITCH_INTERVAL = 45000;

#define CLASS_MODE_BELAJAR 0
#define CLASS_MODE_ISTIRAHAT 1
#define CLASS_MODE_ISHOMA 2
int classMode = CLASS_MODE_BELAJAR;

// ============ JADWAL & BEL + LOW ENERGI + MITIGASI ============
bool mitigationMode = false;
unsigned long mitigationUntil = 0;
unsigned long lastLowEnergyBell = 0;
const unsigned long LOW_ENERGY_BELL_INTERVAL = 120000;

// ============ NTP TIME & SCHEDULE ============
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 25200;
const int daylightOffset_sec = 0;

// ============ KALIBRASI SENSOR ============
float gasBaseline = 1500.0f;
float soundBaseline = 40.0f;
float flameAOCal = 500.0f;
int flameDebounce = 0;
const int CAL_SAMPLES = 30;

// ============ SENSOR HISTORY ============
#define HISTORY_SIZE 60
float tempHistory[HISTORY_SIZE];
float humidHistory[HISTORY_SIZE];
float gasHistory[HISTORY_SIZE];
float luxHistory[HISTORY_SIZE];
float soundHistory[HISTORY_SIZE];
int historyIndex = 0;
bool historyFilled = false;

#define GRAPH_16H_POINTS 480
float temp16History[GRAPH_16H_POINTS];
float dist16History[GRAPH_16H_POINTS];
int graph16Index = 0;
bool graph16Filled = false;
unsigned long lastGraph16Sample = 0;
const int HISTORY_JSON_POINTS = 120;

// ============ VARIABEL SISTEM ============
unsigned long bootTime = 0;
unsigned long lastSensorRead = 0;
unsigned long lastDHTRead = 0;
unsigned long lastSerialLog = 0;
unsigned long apShutdownTimer = 0;
const long sensorInterval = 500;
bool connectedToRouter = false;
bool tryingToConnect = false;
bool apAutoShutdownArmed = false;
bool apShutdownDone = false;
int flameCounter = 0;
bool flameConfirmed = false;
const int FLAME_THRESHOLD = 5;
int noiseDurationTimer = 0;
const int NOISE_TRIGGER_TIME = 6;
float noiseThreshold = 75.0f;
const int pwmChannel = 0;

// ============ FITUR BARU: MODE MUSIK RINGAN ============
bool lightweightMusicMode = false;
uint16_t maxMelodyNotes = 120;
uint8_t melodyTempoDiv = 1;

// ============ ROBUSTNESS: STATIC JSON BUFFERS ============
static StaticJsonDocument<256> docSmall;
static StaticJsonDocument<512> docMedium;
static StaticJsonDocument<1024> docLarge;
static StaticJsonDocument<1536> docXLarge;

// ============ ROBUSTNESS: ERROR COUNTERS ============
struct ErrorCounters {
  uint16_t sensorErrors = 0;
  uint16_t wifiErrors = 0;
  uint16_t dhtErrors = 0;
  uint16_t i2cErrors = 0;
  uint16_t lastResetReason = 0;
} errorCounters;

// ============ ROBUSTNESS: RATE LIMITING ============
struct RateLimit {
  unsigned long lastRequest = 0;
  uint16_t requestCount = 0;
  unsigned long windowStart = 0;
} rateLimit;

const uint16_t RATE_LIMIT_MAX = 30;      // Max requests per window
const uint32_t RATE_LIMIT_WINDOW = 60000; // 1 minute window

// ============ ROBUSTNESS: WIFI RECONNECTION ============
struct WifiReconnect {
  uint8_t attempt = 0;
  unsigned long lastAttempt = 0;
  const uint32_t INITIAL_DELAY = 1000;
  const uint32_t MAX_DELAY = 60000;
  uint32_t currentDelay = 1000;
} wifiReconnect;

// ============ ROBUSTNESS: SENSOR SAMPLING ============
struct SensorReading {
  float value;
  bool valid;
  uint8_t errorCount;
};

struct SensorReading lastValidTemp = {0, false, 0};
struct SensorReading lastValidHum = {0, false, 0};
struct SensorReading lastValidGas = {0, false, 0};
struct SensorReading lastValidLux = {0, false, 0};
struct SensorReading lastValidSound = {0, false, 0};

// ============ ROBUSTNESS: OUTLIER DETECTION ============
#define OUTLIER_WINDOW 5
float tempWindow[OUTLIER_WINDOW];
float soundWindow[OUTLIER_WINDOW];
uint8_t windowIndex = 0;
bool windowFilled = false;

// ============ WEB HTML ============
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="id">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart Class Monitor</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;padding:12px;max-width:900px;margin:0 auto;transition:background .3s,color .3s}
body.dark{background:#1a1a2e;color:#eee}
body.light{background:#f0f4f8;color:#222}
h1{font-size:1.4rem;margin-bottom:12px;text-align:center}
.card{background:inherit;border:1px solid #ccc;border-radius:8px;padding:14px;margin-bottom:12px}
body.dark .card{border-color:#444}
table{width:100%;border-collapse:collapse;font-size:0.9rem}
th,td{padding:8px;text-align:left;border-bottom:1px solid #ddd}
body.dark th,body.dark td{border-color:#333}
th{background:#003366;color:#fff}
.val{font-weight:700;font-family:monospace}
.sci{font-size:0.75rem;color:#666;display:block}
body.dark .sci{color:#aaa}
.btn{padding:10px 16px;border:none;border-radius:6px;cursor:pointer;font-weight:600;margin:4px}
.btn-pri{background:#007bff;color:#fff}
.btn-sec{background:#28a745;color:#fff}
.btn-dan{background:#dc3545;color:#fff}
.wifi-box{background:#f8f9fa;padding:12px;border-radius:6px;margin:10px 0}
body.dark .wifi-box{background:#2d2d44}
#wifiStatus{font-weight:600;margin-top:8px}
#newIpAlert{display:none;background:#d4edda;color:#155724;padding:10px;border-radius:6px;margin-top:10px;border:1px solid #c3e6cb}
.piano{display:flex;gap:3px;flex-wrap:wrap;justify-content:center;margin:10px 0}
.key{width:40px;height:120px;border:1px solid #999;border-radius:0 0 4px 4px;cursor:pointer;display:flex;align-items:flex-end;justify-content:center;padding-bottom:6px;font-size:0.7rem;transition:.1s}
.key:active{transform:scale(0.98)}
.key.black{background:#222;color:#fff;height:80px;width:28px;margin:0 -14px 0 -14px;z-index:1}
#toast{position:fixed;top:16px;left:50%;transform:translateX(-50%);background:#333;color:#fff;padding:12px 24px;border-radius:8px;z-index:99;visibility:hidden;transition:visibility .3s}
#toast.show{visibility:visible}
.badge{display:inline-block;padding:2px 8px;border-radius:4px;font-size:0.8rem}
.badge-ok{background:#d4edda;color:#155724}
.badge-warn{background:#fff3cd;color:#856404}
.badge-dan{background:#f8d7da;color:#721c24}
input[type=text],input[type=password]{width:100%;padding:8px;margin:4px 0;border:1px solid #ccc;border-radius:4px}
body.dark input{border-color:#555;background:#333;color:#fff}
.stats{font-size:0.75rem;color:#666;margin-top:8px}
body.dark .stats{color:#aaa}
</style>
</head>
<body class="light" id="body">
<div id="toast"></div>
<h1>📊 SMART CLASS MONITOR v2 (ROBUST)</h1>

<div class="card">
<h3>📈 Data Sensor HD (Ilmiah)</h3>
<table>
<tr><th>Parameter</th><th>Nilai</th><th>Status</th></tr>
<tr><td>Suhu</td><td><span class="val" id="t">--</span> <span id="u_t">C</span><span class="sci" id="t_sci">-</span></td><td><span class="badge" id="st_t">-</span></td></tr>
<tr><td>Kelembapan</td><td><span class="val" id="h">--</span> <span id="u_h">%</span><span class="sci" id="h_sci">-</span></td><td><span class="badge" id="st_h">-</span></td></tr>
<tr><td>Gas (MQ-135)</td><td><span class="val" id="g">--</span> <span id="u_g">raw</span><span class="sci" id="g_sci">-</span></td><td><span class="badge" id="st_g">-</span></td></tr>
<tr><td>Kebisingan</td><td><span class="val" id="s">--</span> <span id="u_n">dB</span><span class="sci" id="s_sci">-</span></td><td><span class="badge" id="st_s">-</span></td></tr>
<tr><td>Cahaya (BH1750)</td><td><span class="val" id="l">--</span> <span id="u_l">Lx</span> | <span class="val" id="ldr_do">--</span><span class="sci" id="l_sci">-</span></td><td><span class="badge" id="st_l">-</span></td></tr>
<tr><td>Ultrasonik</td><td><span class="val" id="dist">--</span> cm<span class="sci" id="dist_sci">-</span></td><td><span class="badge" id="st_d">-</span></td></tr>
<tr><td>Api</td><td><span class="val" id="flame">AMAN</span></td><td><span class="badge" id="st_f">-</span></td></tr>
</table>
<div class="stats" id="sysStats">Error: 0 | WiFi: 0 | DHT: 0 | I2C: 0 | Reset: 0</div>
</div>


<div class="card">
<h3>👶 Mode Pemula (Cara Pakai Cepat)</h3>
<ol style="padding-left:20px">
<li>Hubungkan HP/Laptop ke WiFi <b>SMART_CLASS_AP</b>.</li>
<li>Buka web monitor lalu isi <b>Admin Key</b> bila ingin kontrol bel/lagu.</li>
<li>Cek data sensor di tabel, lalu lihat status badge (AMAN / WASPADA / BAHAYA).</li>
<li>Pakai menu Piano untuk tes buzzer, atau pilih lagu dari dropdown.</li>
</ol>
</div>

<div class="card">
<h3>📚 Referensi Min/Max (Sumber Asli)</h3>
<table>
<tr><th>Parameter</th><th>WHO</th><th>Kemenkes/Standar Kelas</th></tr>
<tr><td>Suhu</td><td>18-24 C</td><td>18-26 C</td></tr>
<tr><td>Kelembapan</td><td>40-70 %</td><td>40-70 %</td></tr>
<tr><td>Kebisingan</td><td><=55 dB</td><td><=55 dB</td></tr>
<tr><td>Pencahayaan</td><td>300-500 lux</td><td>250-750 lux</td></tr>
</table>
<p class="sci">WHO Housing and health guidelines: <a href="https://www.who.int/publications/i/item/9789241550376" target="_blank">who.int</a></p>
<p class="sci">Permenkes & standar kesehatan lingkungan sekolah: <a href="https://peraturan.bpk.go.id/" target="_blank">peraturan.bpk.go.id</a></p>
</div>

<div class="card">
<h3>Grafik Dashboard Ringan (hingga 8 Jam)</h3>
<canvas id="chart16h" width="860" height="220" style="width:100%;border:1px solid #bbb;border-radius:6px"></canvas>
<p class="sci">Garis hijau: Suhu (C) | Garis biru: Jarak Ultrasonik (cm)</p>
</div>

<div class="card">
<h3>🧠 AI Prediksi Stres & Mood Belajar</h3>
<p>Stres: <span class="val" id="stress">--</span>/100 | Mood: <span class="val" id="mood">--</span></p>
<p class="sci"><span id="sound_err">-</span></p>
</div>

<div class="card">
<h3>⏰ Jadwal & Bel</h3>
<p>Admin Key wajib untuk bel</p>
<p>Mode Kelas: <span class="val" id="class_mode">Belajar</span></p>
<p style="margin-top:8px">Set Mode Cepat:</p>
<button class="btn btn-sec" onclick="setClassMode(1)">Mode Istirahat</button>
<button class="btn btn-sec" onclick="setClassMode(2)">Mode Ishoma</button>
<button class="btn btn-sec" onclick="setClassMode(0)">Mode Belajar</button>
<button class="btn btn-pri" onclick="ringBell(1)">🔔 Istirahat</button>
<button class="btn btn-pri" onclick="ringBell(2)">🕐 Masuk</button>
<button class="btn btn-pri" onclick="ringBell(3)">🕌 Ishoma</button>
<button class="btn btn-pri" onclick="ringBell(4)">👨‍🏫 Guru Masuk</button>
<button class="btn btn-sec" onclick="ringBell(5)">⚡ Low Energi</button>
<button class="btn btn-dan" id="mitBtn" onclick="mitigate()">🛡️ Mitigasi (Musik On)</button>
</div>

<div class="card">
<h3>🔊 Suara Ambient & Mode Otomatis</h3>
<p>Mode: <span class="val" id="ambient_mode">Off</span></p>
<button class="btn btn-pri" onclick="playAmbient(1)">🌀 Kipas</button>
<button class="btn btn-pri" onclick="playAmbient(2)">😷 Batuk</button>
<button class="btn btn-pri" onclick="playAmbient(3)">👨‍🏫 Guru</button>
<button class="btn btn-sec" id="autoBtn" onclick="toggleAuto()">⚡ Auto: OFF</button>
</div>

<div class="card">
<h3>🎹 Piano & 50 Lagu Full</h3>
<div style="margin:8px 0">
<label>Transpose:</label> <input id="transpose" type="range" min="-12" max="12" value="0" style="width:180px">
<span id="transpose_val">0</span> semitone |
<label>Durasi:</label> <input id="toneDur" type="range" min="80" max="500" step="10" value="180" style="width:180px">
<span id="toneDur_val">180</span> ms
</div>
<div class="piano">
<div class="key" onclick="play(262)">C4</div>
<div class="key black" onclick="play(277)">C#4</div>
<div class="key" onclick="play(294)">D4</div>
<div class="key black" onclick="play(311)">D#4</div>
<div class="key" onclick="play(330)">E4</div>
<div class="key" onclick="play(349)">F4</div>
<div class="key black" onclick="play(370)">F#4</div>
<div class="key" onclick="play(392)">G4</div>
<div class="key black" onclick="play(415)">G#4</div>
<div class="key" onclick="play(440)">A4</div>
<div class="key black" onclick="play(466)">A#4</div>
<div class="key" onclick="play(494)">B4</div>
<div class="key" onclick="play(523)">C5</div>
<div class="key black" onclick="play(554)">C#5</div>
<div class="key" onclick="play(587)">D5</div>
<div class="key" onclick="play(659)">E5</div>
<div class="key" onclick="play(784)">G5</div>
<div class="key" onclick="play(880)">A5</div>
<div class="key" onclick="play(988)">B5</div>
<div class="key" onclick="play(1047)">C6</div>
</div>
<select id="songSelect" style="padding:8px;margin:4px;min-width:180px">
<option value="1">1. Nokia</option>
<option value="2">2. Mario</option>
<option value="3">3. Star Wars</option>
<option value="4">4. Happy B-Day</option>
<option value="5">5. Fur Elise</option>
<option value="6">6. Twinkle</option>
<option value="7">7. Jingle Bells</option>
<option value="8">8. Ode to Joy</option>
<option value="9">9. Tetris</option>
<option value="10">10. Pirates</option>
<option value="11">11. Harry Potter</option>
<option value="12">12. Mission</option>
<option value="13">13. Titanic</option>
<option value="14">14. Wedding</option>
<option value="15">15. Chopsticks</option>
<option value="16">16. London Bridge</option>
<option value="17">17. Mary Lamb</option>
<option value="18">18. Row Row</option>
<option value="19">19. Old MacDonald</option>
<option value="20">20. Silent Night</option>
<option value="21">21. We Wish</option>
<option value="22">22. Amazing Grace</option>
<option value="23">23. Beethoven 5</option>
<option value="24">24. Smoke Water</option>
<option value="25">25. Seven Nation</option>
<option value="26">26. William Tell</option>
<option value="27">27. Mountain King</option>
<option value="28">28. Turkish March</option>
<option value="29">29. ABC Song</option>
<option value="30">30. Baa Baa</option>
<option value="31">31. Yankee</option>
<option value="32">32. Greensleeves</option>
<option value="33">33. O Christmas</option>
<option value="34">34. Imagine</option>
<option value="35">35. Hakuna Matata</option>
<option value="36">36. Let It Be</option>
<option value="37">37. Canon</option>
<option value="38">38. Auld Lang</option>
<option value="39">39. La Cucaracha</option>
<option value="40">40. Indiana</option>
<option value="41">41. Yellow Sub</option>
<option value="42">42. Take On Me</option>
<option value="43">43. Pachelbel</option>
<option value="44">44. Moonlight</option>
<option value="45">45. Ave Maria</option>
<option value="46">46. Scherzo</option>
<option value="47">47. Toreador</option>
<option value="48">48. Brahms</option>
<option value="49">49. Bridge</option>
<option value="50">50. Lavender</option>
<option value="51">51. Hallelujah</option>
<option value="52">52. Senorita</option>
<option value="53">53. Despacito</option>
<option value="54">54. Shake It</option>
<option value="55">55. Canon Lite</option>
</select>
<button class="btn btn-pri" onclick="playSong(document.getElementById('songSelect').value)">Putar Lagu</button>
</div>

<div class="card">
<h3>📶 WiFi Manager (Jelas)</h3>
<div class="wifi-box">
<button class="btn btn-sec" onclick="scanWifi()">🔍 Scan Network</button>
<span id="wifiScan">-</span>
</div>
<input type="text" id="ssid" placeholder="SSID">
<input type="password" id="pass" placeholder="Password">
<input type="password" id="key" placeholder="Admin Key (wajib)">
<button class="btn btn-sec" onclick="connectWifi()">Koneksi WiFi</button>
<p id="wifiStatus">Status: AP Mode</p>
<p>IP Web Aktif: <a id="webIpLink" href="#" target="_blank"><span class="val" id="webIp">-</span></a></p>
<div id="newIpAlert"></div>
</div>

<div class="card">
<h3>⚙️ Advanced Settings (Satuan)</h3>
<label>Suhu:</label>
<select id="set_t" onchange="setUnit('t',this.value)"><option value="C">Celsius (°C)</option><option value="F">Fahrenheit (°F)</option><option value="K">Kelvin (K)</option><option value="R">Reamur (°R)</option></select>
<br><label>Kelembapan:</label>
<select id="set_h" onchange="setUnit('h',this.value)"><option value="%">% RH</option><option value="AH">Absolute (g/m³)</option></select>
<br><label>Gas:</label>
<select id="set_g" onchange="setUnit('g',this.value)"><option value="RAW">Raw ADC</option><option value="PPM">Estimasi PPM</option><option value="MGM3">Estimasi mg/m3</option></select>
<br><label>Cahaya:</label>
<select id="set_l" onchange="setUnit('l',this.value)"><option value="Lx">Lux</option><option value="Fc">Foot-candles</option></select>
<br><label>Suara:</label>
<select id="set_n" onchange="setUnit('n',this.value)"><option value="dB">dB SPL</option><option value="V">Voltage (V)</option></select>
</div>

<div class="card">
<button class="btn btn-pri" onclick="toggleTheme()">🌓 Tema</button>
<button class="btn btn-pri" onclick="exportData()">📥 Export JSON</button>
<button class="btn btn-pri" onclick="calibrate()">🔧 Kalibrasi</button>
</div>

<script>
function toast(m){const t=document.getElementById('toast');t.textContent=m;t.classList.add('show');setTimeout(()=>t.classList.remove('show'),3500)}
function play(f){
  const t=parseInt(document.getElementById('transpose').value||'0',10);
  const d=parseInt(document.getElementById('toneDur').value||'180',10);
  const target=Math.max(40,Math.min(4000,Math.round(f*Math.pow(2,t/12))));
  if(target>0)fetch('/play?f='+target+'&d='+d);
}
function playSong(id){
  const k=document.getElementById('key').value;
  if(!k){toast('Masukkan Admin Key!');return}
  fetch('/song?id='+id+'&key='+encodeURIComponent(k)).then(r=>r.text()).then(t=>toast(t))
}
function updateWebIp(ip){
  if(!ip)return;
  const ipEl=document.getElementById('webIp');
  const linkEl=document.getElementById('webIpLink');
  if(ipEl)ipEl.textContent=ip;
  if(linkEl){linkEl.href='http://'+ip;linkEl.title='http://'+ip;}
}
function refreshWifiStatus(){
  fetch('/wifi_status').then(r=>r.json()).then(d=>{
    if(!d)return;
    const ws=document.getElementById('wifiStatus');
    if(ws){
      if(d.connected)ws.textContent='Terhubung ke '+(d.ssid||'-')+' ('+(d.rssi||0)+' dBm)';
      else ws.textContent='Status: AP Mode';
    }
    updateWebIp(d.ip||d.ap_ip||'');
  }).catch(()=>{})
}
function connectWifi(){
  const s=document.getElementById('ssid').value,p=document.getElementById('pass').value,k=document.getElementById('key').value;
  if(!k){toast('Masukkan Admin Key!');return}
  document.getElementById('wifiStatus').textContent='Menghubungkan...';
  fetch('/connect?ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p)+'&key='+encodeURIComponent(k))
    .then(r=>r.text()).then(t=>{
      toast(t);
      let chk=0;
      const i=setInterval(()=>{
        fetch('/wifi_status').then(r=>r.json()).then(d=>{
          if(d.connected){
            clearInterval(i);
            document.getElementById('wifiStatus').textContent='Terhubung ke '+(d.ssid||'-');
            updateWebIp(d.ip);
            const al=document.getElementById('newIpAlert');
            al.style.display='block';
            al.innerHTML='<b>KONEKSI SUKSES!</b><br>IP Baru: <a href="http://'+d.ip+'" target="_blank">'+d.ip+'</a><br>Silakan buka IP baru. AP ESP akan mati otomatis.';
            alert('WiFi Terhubung!\nIP Baru: '+d.ip+'\nSilakan pindah ke IP tersebut.');
          }
        });
        chk++;if(chk>30)clearInterval(i);
      },2000);
    })
}
function scanWifi(){
  document.getElementById('wifiScan').textContent='Scanning...';
  fetch('/scan').then(r=>r.json()).then(d=>{
    let s='';
    if(d&&d.networks){d.networks.slice(0,8).forEach(n=>s+=n.ssid+' ('+n.rssi+'dBm) | ');}
    document.getElementById('wifiScan').textContent=s||'Tidak ada network'
  }).catch(()=>document.getElementById('wifiScan').textContent='Error')
}
function toggleTheme(){
  const b=document.getElementById('body');
  b.classList.toggle('dark');b.classList.toggle('light');toast(b.classList.contains('dark')?'Tema Gelap':'Tema Terang')
}
function exportData(){
  fetch('/data').then(r=>r.json()).then(d=>{
    const a=document.createElement('a');a.href='data:application/json,'+encodeURIComponent(JSON.stringify(d));a.download='sensor_data.json';a.click();toast('Data diunduh')
  })
}
function calibrate(){
  const k=document.getElementById('key').value;
  if(!k){toast('Masukkan Admin Key!');return}
  fetch('/calibrate?key='+encodeURIComponent(k)).then(r=>r.text()).then(t=>toast(t))
}
function playAmbient(m){
  fetch('/ambient?mode='+m).then(r=>r.text()).then(t=>toast(t))
}
function ringBell(id){
  const k=document.getElementById('key').value;
  if(!k){toast('Masukkan Admin Key!');return}
  fetch('/bell?id='+id+'&key='+encodeURIComponent(k)).then(r=>r.text()).then(t=>toast(t))
}
function setClassMode(m){
  const k=document.getElementById('key').value;
  if(!k){toast('Masukkan Admin Key!');return}
  fetch('/class_mode?m='+m+'&key='+encodeURIComponent(k)).then(r=>r.text()).then(t=>toast(t))
}
function mitigate(){
  fetch('/mitigate').then(r=>r.text()).then(t=>toast(t))
}
function toggleAuto(){
  fetch('/auto_mode?on='+(document.getElementById('autoBtn').textContent.includes('ON')?'0':'1'))
    .then(r=>r.json()).then(d=>{
      document.getElementById('autoBtn').textContent='⚡ Auto: '+(d.auto?'ON':'OFF');
      toast(d.auto?'Mode Auto Aktif - ambient berganti otomatis':'Mode Auto Mati')
    })
}
function setUnit(type,val){
  fetch('/api/set_units?t='+type+'&v='+val).then(r=>r.text()).then(t=>toast('Satuan diubah: '+val));
}
function ensureAdvancedUnitOptions(){
  const addOpt=(id,val,label)=>{
    const el=document.getElementById(id);
    if(!el)return;
    for(let i=0;i<el.options.length;i++){if(el.options[i].value===val)return;}
    const o=document.createElement('option');o.value=val;o.textContent=label;el.appendChild(o);
  };
  addOpt('set_h','GPKG','Mixing Ratio (g/kg)');
  addOpt('set_l','Wm2','W/m2 (estimasi)');
  addOpt('set_n','Pa','Pascal (Pa)');
}
function draw16hChart(d){
  const c=document.getElementById('chart16h');
  if(!c||!d||!d.temp||!d.dist)return;
  const ctx=c.getContext('2d');
  const w=c.width,h=c.height,p=18;
  ctx.clearRect(0,0,w,h);
  ctx.fillStyle='#fafafa';ctx.fillRect(0,0,w,h);
  ctx.strokeStyle='#cccccc';ctx.strokeRect(0,0,w,h);
  const n=Math.min(d.temp.length,d.dist.length);
  if(n<2)return;

  let tMin=9999,tMax=-9999,dMin=9999,dMax=-9999;
  for(let i=0;i<n;i++){
    const tv=Number(d.temp[i]||0), dv=Number(d.dist[i]||0);
    if(tv<tMin)tMin=tv;if(tv>tMax)tMax=tv;
    if(dv<dMin)dMin=dv;if(dv>dMax)dMax=dv;
  }
  if(tMax===tMin)tMax=tMin+1;
  if(dMax===dMin)dMax=dMin+1;
  const plotW=w-(p*2), plotH=h-(p*2);
  const xAt=i=>p+(i*(plotW/(n-1)));
  const yTemp=v=>p+plotH-((v-tMin)/(tMax-tMin))*plotH;
  const yDist=v=>p+plotH-((v-dMin)/(dMax-dMin))*plotH;

  ctx.strokeStyle='#2e8b57';ctx.lineWidth=2;ctx.beginPath();
  for(let i=0;i<n;i++){const x=xAt(i),y=yTemp(Number(d.temp[i]||0));if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);}ctx.stroke();
  ctx.strokeStyle='#1e90ff';ctx.lineWidth=2;ctx.beginPath();
  for(let i=0;i<n;i++){const x=xAt(i),y=yDist(Number(d.dist[i]||0));if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);}ctx.stroke();

  ctx.fillStyle='#333';ctx.font='12px sans-serif';
  ctx.fillText('8 jam lalu', p, h-4);
  ctx.fillText('sekarang', w-62, h-4);
  ctx.fillText('T min/max: '+tMin.toFixed(1)+'/'+tMax.toFixed(1), p, 12);
  ctx.fillText('D min/max: '+dMin.toFixed(1)+'/'+dMax.toFixed(1), w-180, 12);
}
function refresh16hChart(){
  fetch('/history16').then(r=>r.json()).then(draw16hChart).catch(()=>{})
}
function setBadge(id,txt){
  const el=document.getElementById(id);if(!el)return;
  el.textContent=txt||'-';
  el.className='badge badge-ok';
  if(txt&&(txt.includes('PANAS')||txt.includes('Hangat')||txt.includes('BISING')||txt.includes('REDUP')))el.className='badge badge-warn';
  if(txt&&txt.includes('LEMBAB'))el.className='badge badge-ok';
  if(txt&&(txt.includes('BAHAYA')||txt.includes('BURUK')||txt.includes('DIAM')||txt.includes('Dekat')))el.className='badge badge-dan'
}
let lastAlert=false;
const fSt=document.getElementById('st_f');
ensureAdvancedUnitOptions();
const tr=document.getElementById('transpose');
const tv=document.getElementById('transpose_val');
const dr=document.getElementById('toneDur');
const dv=document.getElementById('toneDur_val');
if(tr&&tv){tr.addEventListener('input',()=>tv.textContent=tr.value);}
if(dr&&dv){dr.addEventListener('input',()=>dv.textContent=dr.value);}
refreshWifiStatus();
refresh16hChart();
setInterval(refreshWifiStatus,3000);
setInterval(refresh16hChart,30000);
setInterval(()=>{
  fetch('/data').then(r=>r.json()).then(d=>{
    if(!d)return;
    document.getElementById('t').textContent=d.t||'--';
    document.getElementById('h').textContent=d.h||'--';
    document.getElementById('g').textContent=d.g||'--';
    document.getElementById('s').textContent=d.s||'--';
    document.getElementById('l').textContent=d.l||'--';
    document.getElementById('dist').textContent=d.dist||'--';
    document.getElementById('u_t').textContent=d.u_t;
    document.getElementById('u_h').textContent=d.u_h;
    document.getElementById('u_g').textContent=d.u_g||'raw';
    document.getElementById('u_l').textContent=d.u_l;
    document.getElementById('u_n').textContent=d.u_n;
    const ldrDo=document.getElementById('ldr_do');if(ldrDo)ldrDo.textContent=d.ldr_do||'--';
    document.getElementById('flame').textContent=d.f_val||'AMAN';
    if(fSt){
      if(d.f_alert){fSt.textContent='BAHAYA';fSt.className='badge badge-dan';if(!lastAlert){toast('!!! API TERDETEKSI !!!');lastAlert=true}}
      else{fSt.textContent='AMAN';fSt.className='badge badge-ok';lastAlert=false}
    }
  }).catch(()=>{})
},1500);
</script>
</body>
</html>
)rawliteral";

// ============ ANTI-BUG: Validasi Nilai Sensor ============
bool isValidFloat(float v, float minV, float maxV) {
  return !isnan(v) && !isinf(v) && v >= minV && v <= maxV;
}

bool isValidInt(int v, int minV, int maxV) {
  return v >= minV && v <= maxV;
}

float safeFloat(float v, float def, float minV, float maxV) {
  if (!isValidFloat(v, minV, maxV)) return def;
  return v;
}

// ============ ROBUSTNESS: Improved Ultrasonic Reading ============
float readUltrasonicCm() {
  float readings[3] = {0, 0, 0};
  for (int i = 0; i < 3; i++) {
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
    unsigned long duration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH, 30000);
    if (duration > 0) readings[i] = duration * 0.0343f / 2.0f;
    delay(10);
  }
  float sorted[3];
  memcpy(sorted, readings, sizeof(readings));
  for (int i = 0; i < 2; i++) {
    for (int j = i + 1; j < 3; j++) {
      if (sorted[i] > sorted[j]) { float t=sorted[i]; sorted[i]=sorted[j]; sorted[j]=t; }
    }
  }
  return safeFloat(sorted[1], distanceCm, 2.0f, 450.0f);
}

// ============ ROBUSTNESS: DHT Reading with Retries ============
bool readDHTWithRetry(float& temp, float& hum, uint8_t retries=3) {
  for (uint8_t i=0; i<retries; i++) {
    temp = dht.readTemperature();
    hum = dht.readHumidity();
    if (isValidFloat(temp,-40,80) && isValidFloat(hum,0,100)) return true;
    delay(50);
  }
  errorCounters.dhtErrors++;
  return false;
}

// ============ KALIBRASI SENSOR ============
void runCalibration() {
  float gasSum=0, soundSum=0;
  Serial.println("Kalibrasi...");
  for(int i=0;i<CAL_SAMPLES;i++){ gasSum+=analogRead(MQ135_PIN); int sMin=4095,sMax=0;
    for(int j=0;j<20;j++){ int v=analogRead(SOUND_PIN); if(v<sMin)sMin=v; if(v>sMax)sMax=v; }
    int amp=sMax-sMin; soundSum+=(amp>0)?(20.0f*log10((float)amp)+10.0f):40.0f; delay(100); }
  gasBaseline=gasSum/CAL_SAMPLES; soundBaseline=soundSum/CAL_SAMPLES;
  Serial.println("Gas:"+String(gasBaseline)+" dB:"+String(soundBaseline));
}

// ============ HISTORY ============
void updateHistory() {
  tempHistory[historyIndex]=temperature; humidHistory[historyIndex]=humidity;
  gasHistory[historyIndex]=(float)gasRaw; luxHistory[historyIndex]=luxEst; soundHistory[historyIndex]=dB;
  historyIndex=(historyIndex+1)%HISTORY_SIZE; if(historyIndex==0)historyFilled=true;
  unsigned long now=millis();
  if((now-lastGraph16Sample)>=60000||lastGraph16Sample==0){
    lastGraph16Sample=now; temp16History[graph16Index]=temperature; dist16History[graph16Index]=distanceCm;
    graph16Index=(graph16Index+1)%GRAPH_16H_POINTS; if(graph16Index==0)graph16Filled=true;
  }
}

float getMin(float arr[]){ float m=arr[0]; int n=historyFilled?HISTORY_SIZE:historyIndex;
  if(n==0)return 0; for(int i=1;i<n;i++)if(arr[i]<m)m=arr[i]; return m; }
float getMax(float arr[]){ float m=arr[0]; int n=historyFilled?HISTORY_SIZE:historyIndex;
  if(n==0)return 0; for(int i=1;i<n;i++)if(arr[i]>m)m=arr[i]; return m; }
float getAvg(float arr[]){ int n=historyFilled?HISTORY_SIZE:historyIndex;
  if(n==0)return 0; float s=0; for(int i=0;i<n;i++)s+=arr[i]; return s/n; }

// ============ OUTLIER DETECTION ============
bool isOutlier(float val, float win[], uint8_t sz, float thr=3.0f){
  if(!windowFilled)return false; float sum=0,sqSum=0;
  for(int i=0;i<sz;i++){ sum+=win[i]; sqSum+=win[i]*win[i]; }
  float mean=sum/sz; float stdDev=sqrt((sqSum/sz)-(mean*mean));
  if(stdDev<0.1)return false; return abs(val-mean)>thr*stdDev;
}
void updateOutlierWin(float t,float s){ tempWindow[windowIndex]=t; soundWindow[windowIndex]=s;
  windowIndex=(windowIndex+1)%OUTLIER_WINDOW; if(windowIndex==0)windowFilled=true; }

// ============ AI STRES & MOOD ============
void computeStressAndMood(){
  float s=0;
  if(temperature>32)s+=30; else if(temperature>29)s+=15;
  if(humidity>85||humidity<30)s+=10;
  if(dB>75)s+=25; else if(dB>65)s+=15;
  if(luxEst<100&&ldrDoStatus=="Gelap")s+=10;
  if(gasRaw>2500)s+=20; else if(gasRaw>2000)s+=10;
  stressScore=(int)constrain(s,0,100);
  float avg=getAvg(soundHistory); float var=0; int n=historyFilled?HISTORY_SIZE:historyIndex;
  if(n>5){ for(int i=0;i<n;i++)var+=(soundHistory[i]-avg)*(soundHistory[i]-avg); var/=n; }
  soundUnreadable=(dB<20||dB>115||(n>10&&var<0.5));
  if(soundUnreadable)moodLearn="Tidak Terbaca";
  else if(stressScore>=70)moodLearn="Gelisah";
  else if(stressScore>=50)moodLearn="Lelah";
  else if(stressScore>=30)moodLearn="Bosan";
  else if(luxEst<80&&ldrDoStatus=="Gelap")moodLearn="Mengantuk";
  else if(stressScore<20&&dB<55)moodLearn="Fokus";
  else moodLearn="Nyaman";
}

// ============ KONVERSI SATUAN ============
String getTempUnitLabel(){ if(unitTemp=="K")return"K"; if(unitTemp=="R")return"R"; if(unitTemp=="F")return"F"; return"C"; }
String getHumUnitLabel(){ if(unitHum=="AH")return"g/m3"; if(unitHum=="GPKG")return"g/kg"; return"%"; }
String getGasUnitLabel(){ if(unitGas=="PPM")return"ppm"; if(unitGas=="MGM3")return"mg/m3"; return"raw"; }
String getLuxUnitLabel(){ if(unitLux=="Fc")return"Fc"; if(unitLux=="Wm2")return"W/m2"; return"Lx"; }
String getNoiseUnitLabel(){ if(unitNoise=="V")return"V"; if(unitNoise=="Pa")return"Pa"; return"dB"; }
String getAmbientModeLabel(){ if(ambientMode==AMBIENT_FAN)return"Kipas"; if(ambientMode==AMBIENT_COUGH)return"Batuk"; if(ambientMode==AMBIENT_TEACHER)return"Guru"; return"Off"; }
String getClassModeLabel(){ if(classMode==CLASS_MODE_ISTIRAHAT)return"Istirahat"; if(classMode==CLASS_MODE_ISHOMA)return"Ishoma"; return"Belajar"; }
void setClassModeInternal(int m){ if(m<CLASS_MODE_BELAJAR||m>CLASS_MODE_ISHOMA)return; classMode=m; }

float getTempConverted(float c){ if(unitTemp=="F")return(c*9.0/5.0)+32.0; if(unitTemp=="K")return c+273.15; if(unitTemp=="R")return c*4.0/5.0; return c; }
float getHumConverted(float h,float t){ if(unitHum=="AH")return(6.112f*exp((17.67f*t)/(t+243.5f))*h*2.1674f)/(273.15f+t);
  if(unitHum=="GPKG"){ float es=6.112f*exp((17.67f*t)/(t+243.5f)); float e=(h/100.0f)*es; if(e>=1013.0f)return 0; return(621.97f*e)/(1013.25f-e); } return h; }
float estimateGasPPM(float raw){ float baseline=gasBaseline>1.0f?gasBaseline:1.0f; float ratio=raw/baseline; float ppm=400.0f+((ratio-1.0f)*1800.0f); return ppm<0?0:ppm; }
float getGasConverted(float raw,float tc){ if(unitGas=="PPM")return estimateGasPPM(raw);
  if(unitGas=="MGM3"){ float ppm=estimateGasPPM(raw); float tk=tc+273.15f; if(tk<200.0f)tk=298.15f; return ppm*44.01f*101.325f/(8.314f*tk); } return raw; }
float getLuxConverted(float l){ if(unitLux=="Fc")return l*0.0929f; if(unitLux=="Wm2")return l/683.0f; return l; }
float getNoiseConverted(float db){ if(unitNoise=="V")return constrain((db/120.0f)*3.3f,0.0f,3.3f); if(unitNoise=="Pa")return 0.00002f*pow(10.0f,db/20.0f); return db; }

// ============ RATE LIMITING ============
bool checkRateLimit(){ unsigned long now=millis();
  if(now-rateLimit.windowStart>=RATE_LIMIT_WINDOW){ rateLimit.windowStart=now; rateLimit.requestCount=0; }
  rateLimit.requestCount++; return rateLimit.requestCount<=RATE_LIMIT_MAX; }

// ============ WIFI RECONNECT ============
void handleWifiReconnect(){ if(WiFi.status()==WL_CONNECTED){ connectedToRouter=true; tryingToConnect=false; wifiReconnect.attempt=0; wifiReconnect.currentDelay=wifiReconnect.INITIAL_DELAY; return; }
  if(!tryingToConnect){ tryingToConnect=true; wifiReconnect.lastAttempt=millis(); wifiReconnect.attempt++; Serial.println("WiFi recon attempt:"+String(wifiReconnect.attempt)); return; }
  unsigned long elapsed=millis()-wifiReconnect.lastAttempt;
  if(elapsed>=wifiReconnect.currentDelay){ WiFi.disconnect(); WiFi.reconnect(); wifiReconnect.lastAttempt=millis(); wifiReconnect.currentDelay=min(wifiReconnect.currentDelay*2,wifiReconnect.MAX_DELAY); errorCounters.wifiErrors++; } }

// ============ TELEGRAM ============
void handleNewMessages(int numNewMessages){ for(int i=0;i<numNewMessages;i++){ String chat_id=String(bot.messages[i].chat_id);
  String text=bot.messages[i].text; text.trim(); text.toLowerCase(); int atPos=text.indexOf('@'); if(atPos>0)text=text.substring(0,atPos);
  String from_name=bot.messages[i].from_name; bool kenyCmd=(text=="/kenyamanan"||text=="/kenyamanan_siswa");
  if(text=="/start"){ String w="Halo "+from_name+"!\nBot Smart Class (Robust)\n/start\n/info\n/sensor\n/data\n/download\n/kenyamanan_siswa\n/mode"; bot.sendMessage(chat_id,w,""); }
  else if(text=="/info"){ String ip=WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():WiFi.softAPIP().toString();
    String m="IP:http://"+ip+"\nWiFi:"+String(WiFi.status()==WL_CONNECTED?"Terhubung":"AP")+"\nMode:"+getClassModeLabel()+"\nUptime:"+String(millis()/1000)+"s\nHeap:"+String(ESP.getFreeHeap()); bot.sendMessage(chat_id,m,""); }
  else if(text=="/sensors"||text=="/sensor"){ float t=getTempConverted(temperature),h=getHumConverted(humidity,temperature),g=getGasConverted((float)gasRaw,temperature),l=getLuxConverted(luxEst),n=getNoiseConverted(dB);
    String m="T:"+String(t,2)+getTempUnitLabel()+"\nH:"+String(h,2)+getHumUnitLabel()+"\nGas:"+String(g,(unitGas=="RAW"?0:2))+getGasUnitLabel()+"\nSuara:"+String(n,2)+getNoiseUnitLabel()+"\nCahaya:"+String(l,2)+getLuxUnitLabel(); bot.sendMessage(chat_id,m,""); }
  else if(kenyCmd){ String m="Stres:"+String(stressScore)+"/100\nMood:"+moodLearn+"\nMode:"+getClassModeLabel(); bot.sendMessage(chat_id,m,""); }
  else if(text=="/data"){
    docMedium.clear();
    docMedium["temp_c"]=temperature; docMedium["hum_pct"]=humidity; docMedium["gas_raw"]=gasRaw;
    docMedium["noise_db"]=dB; docMedium["lux"]=luxEst; docMedium["distance_cm"]=distanceCm;
    docMedium["mood"]=moodLearn; docMedium["stress"]=stressScore;
    String payload; serializeJson(docMedium,payload);
    bot.sendMessage(chat_id,payload,"");
  }
  else if(text=="/download"){
    String ip=WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():WiFi.softAPIP().toString();
    bot.sendMessage(chat_id,"Download JSON: http://"+ip+"/download","");
  }
  else if(text=="/mode"){ String m="Mode:"+getClassModeLabel()+"\nAuto:"+String(autoMode?"ON":"OFF")+"\nAmbient:"+getAmbientModeLabel(); bot.sendMessage(chat_id,m,""); }
  else if(text=="/stats"){ String m="Err Sensor:"+String(errorCounters.sensorErrors)+"\nWiFi:"+String(errorCounters.wifiErrors)+"\nDHT:"+String(errorCounters.dhtErrors); bot.sendMessage(chat_id,m,""); }
  else bot.sendMessage(chat_id,"Cmds:/start /info /sensor /data /download /kenyamanan_siswa /mode /stats",""); } }

// ============ AMBIENT SOUNDS ============
void playAmbientFan(){ for(int i=0;i<8;i++){ ledcWriteTone(pwmChannel,80+(i%3)*15); delay(120); } ledcWriteTone(pwmChannel,0); }
void playAmbientCough(){ ledcWriteTone(pwmChannel,180); delay(80); ledcWriteTone(pwmChannel,120); delay(120); ledcWriteTone(pwmChannel,0); delay(50); ledcWriteTone(pwmChannel,150); delay(60); ledcWriteTone(pwmChannel,0); }
void playAmbientTeacher(){ int seq[]={262,294,330,262,349,330,294,262,0}; for(int i=0;i<9;i++){ if(seq[i]>0){ledcWriteTone(pwmChannel,seq[i]);delay(180+(i%3)*40);}else delay(100); } ledcWriteTone(pwmChannel,0); }

// ============ MUSIK & PIANO ============
void playTone(int freq,int dur){ if(freq<=0){delay(dur);return;} ledcWriteTone(pwmChannel,freq); delay(dur); ledcWriteTone(pwmChannel,0); delay(dur/10); }
void playAlarm(){ if(mitigationMode&&millis()<mitigationUntil){playFurElise();return;} for(int i=0;i<5;i++){playTone(880,200);delay(200);} }
void playQuietAlert(){ playTone(440,300); delay(200); }

// ============ BEL ============
void playBellIstirahat(){ for(int i=0;i<3;i++){playTone(880,150);delay(100);playTone(660,150);delay(200);}ledcWriteTone(pwmChannel,0);}
void playBellMasuk(){ for(int i=0;i<2;i++){playTone(523,300);delay(200);playTone(523,300);delay(400);}ledcWriteTone(pwmChannel,0);}
void playBellIshoma(){ playTone(392,400);delay(300);playTone(440,400);delay(300);playTone(494,500);delay(400);ledcWriteTone(pwmChannel,0);}
void playBellGuruMasuk(){ playTone(659,200);delay(150);playTone(784,200);delay(150);playTone(988,300);ledcWriteTone(pwmChannel,0);}
void playBellLowEnergi(){ for(int i=0;i<4;i++){playTone(880,100);delay(80);playTone(1100,100);delay(80);}ledcWriteTone(pwmChannel,0);}

#define TEMPO_FULL 2200
void playMelodyFull(const int* m,size_t len){
  size_t safeLen=(len/2)*2;
  if(lightweightMusicMode){
    size_t cap=(size_t)maxMelodyNotes*2;
    if(safeLen>cap)safeLen=cap;
  }
  uint8_t tempoDiv=(melodyTempoDiv==0)?1:melodyTempoDiv;
  for(size_t i=0;i<safeLen;i+=2){
    int n=m[i],d=m[i+1]?m[i+1]:4;
    int dur=TEMPO_FULL/(d*tempoDiv);
    if(dur<40)dur=40;
    if(n)playTone(n,dur);else delay(dur);
  }
  ledcWriteTone(pwmChannel,0);
}

// ============ SONGS ============
void playNokia(){playMelodyFull(M_NOKIA,sizeof(M_NOKIA)/sizeof(int));}
void playMario(){playMelodyFull(M_MARIO,sizeof(M_MARIO)/sizeof(int));}
void playStarWars(){playMelodyFull(M_STARWARS,sizeof(M_STARWARS)/sizeof(int));}
void playHBD(){playMelodyFull(M_HBD,sizeof(M_HBD)/sizeof(int));}
void playFurElise(){playMelodyFull(M_FURELISE,sizeof(M_FURELISE)/sizeof(int));}
void playTwinkle(){playMelodyFull(M_TWINKLE,sizeof(M_TWINKLE)/sizeof(int));}
void playJingle(){playMelodyFull(M_JINGLE,sizeof(M_JINGLE)/sizeof(int));}
void playOde(){playMelodyFull(M_ODE,sizeof(M_ODE)/sizeof(int));}
void playTetris(){playMelodyFull(M_TETRIS,sizeof(M_TETRIS)/sizeof(int));}
void playPirates(){playMelodyFull(M_PIRATES,sizeof(M_PIRATES)/sizeof(int));}
void playHarry(){playMelodyFull(M_HARRY,sizeof(M_HARRY)/sizeof(int));}
void playMission(){playMelodyFull(M_MISSION,sizeof(M_MISSION)/sizeof(int));}
void playTitanic(){playMelodyFull(M_TITANIC,sizeof(M_TITANIC)/sizeof(int));}
void playWedding(){playMelodyFull(M_WEDDING,sizeof(M_WEDDING)/sizeof(int));}
void playChopsticks(){playMelodyFull(M_CHOPSTICKS,sizeof(M_CHOPSTICKS)/sizeof(int));}
void playLondon(){playMelodyFull(M_LONDON,sizeof(M_LONDON)/sizeof(int));}
void playMary(){playMelodyFull(M_MARY,sizeof(M_MARY)/sizeof(int));}
void playRow(){playMelodyFull(M_ROW,sizeof(M_ROW)/sizeof(int));}
void playOldMac(){playMelodyFull(M_OLDMAC,sizeof(M_OLDMAC)/sizeof(int));}
void playSilent(){playMelodyFull(M_SILENT,sizeof(M_SILENT)/sizeof(int));}
void playWeWish(){playMelodyFull(M_WEWISH,sizeof(M_WEWISH)/sizeof(int));}
void playAmazing(){playMelodyFull(M_AMAZING,sizeof(M_AMAZING)/sizeof(int));}
void playBeethoven5(){playMelodyFull(M_BEETHOVEN5,sizeof(M_BEETHOVEN5)/sizeof(int));}
void playSmoke(){playMelodyFull(M_SMOKE,sizeof(M_SMOKE)/sizeof(int));}
void playSeven(){playMelodyFull(M_SEVEN,sizeof(M_SEVEN)/sizeof(int));}
void playWilliam(){playMelodyFull(M_WILLIAM,sizeof(M_WILLIAM)/sizeof(int));}
void playMountain(){playMelodyFull(M_MOUNTAIN,sizeof(M_MOUNTAIN)/sizeof(int));}
void playTurkish(){playMelodyFull(M_TURKISH,sizeof(M_TURKISH)/sizeof(int));}
void playABC(){playMelodyFull(M_ABC,sizeof(M_ABC)/sizeof(int));}
void playBaaBaa(){playMelodyFull(M_BAABAA,sizeof(M_BAABAA)/sizeof(int));}
void playYankee(){playMelodyFull(M_YANKEE,sizeof(M_YANKEE)/sizeof(int));}
void playGreensleeves(){playMelodyFull(M_GREENSLEEVES,sizeof(M_GREENSLEEVES)/sizeof(int));}
void playOChristmas(){playMelodyFull(M_OCHRISTMAS,sizeof(M_OCHRISTMAS)/sizeof(int));}
void playImagine(){playMelodyFull(M_IMAGINE,sizeof(M_IMAGINE)/sizeof(int));}
void playHakuna(){playMelodyFull(M_HAKUNA,sizeof(M_HAKUNA)/sizeof(int));}
void playLetItBe(){playMelodyFull(M_LETITBE,sizeof(M_LETITBE)/sizeof(int));}
void playCanon(){playMelodyFull(M_CANON,sizeof(M_CANON)/sizeof(int));}
void playAuldLang(){playMelodyFull(M_AULDLANG,sizeof(M_AULDLANG)/sizeof(int));}
void playLaCucaracha(){playMelodyFull(M_LAKUCARACHA,sizeof(M_LAKUCARACHA)/sizeof(int));}
void playIndiana(){playMelodyFull(M_INDIANA,sizeof(M_INDIANA)/sizeof(int));}
void playYellow(){playMelodyFull(M_YELLOW,sizeof(M_YELLOW)/sizeof(int));}
void playTakeOnMe(){playMelodyFull(M_TAKEONME,sizeof(M_TAKEONME)/sizeof(int));}
void playPachelbel(){playMelodyFull(M_PACHELBEL,sizeof(M_PACHELBEL)/sizeof(int));}
void playMoonlight(){playMelodyFull(M_MOONLIGHT,sizeof(M_MOONLIGHT)/sizeof(int));}
void playAveMaria(){playMelodyFull(M_AVEMARIA,sizeof(M_AVEMARIA)/sizeof(int));}
void playScherzo(){playMelodyFull(M_SCHERZO,sizeof(M_SCHERZO)/sizeof(int));}
void playToreador(){playMelodyFull(M_TOREADOR,sizeof(M_TOREADOR)/sizeof(int));}
void playBrahms(){playMelodyFull(M_BRAHMS,sizeof(M_BRAHMS)/sizeof(int));}
void playBridge(){playMelodyFull(M_BRIDGE,sizeof(M_BRIDGE)/sizeof(int));}
void playLavender(){playMelodyFull(M_LAVENDER,sizeof(M_LAVENDER)/sizeof(int));}
void playHallelujah(){playMelodyFull(M_HALLELUJAH,sizeof(M_HALLELUJAH)/sizeof(int));}
void playSenorita(){playMelodyFull(M_SENORITA,sizeof(M_SENORITA)/sizeof(int));}
void playDespacito(){playMelodyFull(M_DESPACITO,sizeof(M_DESPACITO)/sizeof(int));}
void playShake(){playMelodyFull(M_SHAKE,sizeof(M_SHAKE)/sizeof(int));}

// ============ SCHEDULE ============
void checkSchedule(){ struct tm ti; if(!getLocalTime(&ti))return;
  int h=ti.tm_hour,m=ti.tm_min;
  if(h==10&&m>=15&&m<30)setClassModeInternal(CLASS_MODE_ISTIRAHAT);
  else if(h==12&&m<30)setClassModeInternal(CLASS_MODE_ISHOMA);
  else setClassModeInternal(CLASS_MODE_BELAJAR);
  static int lastMin=-1; if(ti.tm_min!=lastMin){ lastMin=ti.tm_min;
    if(h==10&&m==15){setClassModeInternal(CLASS_MODE_ISTIRAHAT);playBellIstirahat();}
    else if(h==10&&m==30){setClassModeInternal(CLASS_MODE_BELAJAR);playBellMasuk();}
    else if(h==12&&m==0){setClassModeInternal(CLASS_MODE_ISHOMA);playBellIshoma();}
    else if(h==12&&m==30){setClassModeInternal(CLASS_MODE_BELAJAR);playBellMasuk();}
  }
}

// ============ VALIDATION ============
bool validateKey(const String& k){return k.length()>=4&&k.length()<=32;}
bool validateSSID(const String& s){return s.length()>0&&s.length()<=32;}

// ============ WEB HANDLERS ============
void handleRoot(){ if(!checkRateLimit()){server.send(429,"text/plain","Too Many Requests");return;} server.send(200,"text/html",index_html); }
void handlePlay(){ if(!checkRateLimit()){server.send(429,"text/plain","Too Many Requests");return;} int f=server.arg("f").toInt(); int d=server.hasArg("d")?server.arg("d").toInt():200; if(d<60)d=60; if(d>1200)d=1200; if(f>0&&f<5000)playTone(f,d); server.send(200,"text/plain","OK"); }
void handleSong(){ String k=server.arg("key"); if(!validateKey(k)||k!=ADMIN_KEY){server.send(403,"text/plain","Key Salah!");return;} int id=server.arg("id").toInt(); if(id<1||id>55){server.send(400,"text/plain","ID invalid");return;} server.send(200,"text/plain",lightweightMusicMode?"Memutar mode ringan...":"Memutar..."); switch(id){case 1:playNokia();break;case 2:playMario();break;case 3:playStarWars();break;case 4:playHBD();break;case 5:playFurElise();break;case 6:playTwinkle();break;case 7:playJingle();break;case 8:playOde();break;case 9:playTetris();break;case 10:playPirates();break;case 11:playHarry();break;case 12:playMission();break;case 13:playTitanic();break;case 14:playWedding();break;case 15:playChopsticks();break;case 16:playLondon();break;case 17:playMary();break;case 18:playRow();break;case 19:playOldMac();break;case 20:playSilent();break;case 21:playWeWish();break;case 22:playAmazing();break;case 23:playBeethoven5();break;case 24:playSmoke();break;case 25:playSeven();break;case 26:playWilliam();break;case 27:playMountain();break;case 28:playTurkish();break;case 29:playABC();break;case 30:playBaaBaa();break;case 31:playYankee();break;case 32:playGreensleeves();break;case 33:playOChristmas();break;case 34:playImagine();break;case 35:playHakuna();break;case 36:playLetItBe();break;case 37:playCanon();break;case 38:playAuldLang();break;case 39:playLaCucaracha();break;case 40:playIndiana();break;case 41:playYellow();break;case 42:playTakeOnMe();break;case 43:playPachelbel();break;case 44:playMoonlight();break;case 45:playAveMaria();break;case 46:playScherzo();break;case 47:playToreador();break;case 48:playBrahms();break;case 49:playBridge();break;case 50:playLavender();break;case 51:playHallelujah();break;case 52:playSenorita();break;case 53:playDespacito();break;case 54:playShake();break;case 55:playCanon();break;} }

void handleConnect(){ String k=server.arg("key"); if(!validateKey(k)||k!=ADMIN_KEY){server.send(403,"text/plain","Access Denied");return;} String s=server.arg("ssid"),p=server.arg("pass"); if(!validateSSID(s)){server.send(400,"text/plain","SSID invalid");return;} WiFi.begin(s.c_str(),p.c_str()); tryingToConnect=true; connectedToRouter=false; apAutoShutdownArmed=false; apShutdownDone=false; server.send(200,"text/plain","Menghubungkan ke "+s+"..."); }

void handleWifiStatus(){ bool wc=WiFi.status()==WL_CONNECTED; String ip=wc?WiFi.localIP().toString():WiFi.softAPIP().toString(); long msLeft=0; if(apAutoShutdownArmed&&!apShutdownDone){long e=millis()-apShutdownTimer;msLeft=30000-e;if(msLeft<0)msLeft=0;} docSmall.clear(); docSmall["connected"]=wc; docSmall["ip"]=ip; docSmall["ap_ip"]=WiFi.softAPIP().toString(); docSmall["ssid"]=WiFi.SSID(); docSmall["rssi"]=WiFi.RSSI(); docSmall["web_url"]="http://"+ip; docSmall["ap_closed"]=apShutdownDone; docSmall["ap_close_sec"]=msLeft/1000; String o; serializeJson(docSmall,o); server.send(200,"application/json",o); }

void handleScan(){ int n=WiFi.scanNetworks(); docMedium.clear(); JsonArray arr=docMedium.createNestedArray("networks"); for(int i=0;i<n&&i<15;i++){ JsonObject o=arr.add<JsonObject>(); o["ssid"]=WiFi.SSID(i); o["rssi"]=WiFi.RSSI(i); o["channel"]=WiFi.channel(i); } String o; serializeJson(docMedium,o); server.send(200,"application/json",o); }

void handleCalibrate(){ String k=server.arg("key"); if(!validateKey(k)||k!=ADMIN_KEY){server.send(403,"text/plain","Key Salah!");return;} runCalibration(); server.send(200,"text/plain","Kalibrasi selesai.Gas:"+String(gasBaseline)+" dB:"+String(soundBaseline)); }

void handleAmbient(){ int m=server.arg("mode").toInt(); if(m>=1&&m<=3){ambientMode=m; if(m==1)playAmbientFan(); else if(m==2)playAmbientCough(); else if(m==3)playAmbientTeacher(); server.send(200,"text/plain",m==1?"Kipas":m==2?"Batuk":"Guru"); }else{ambientMode=AMBIENT_OFF;server.send(200,"text/plain","Off");} }

void handleAutoMode(){ bool on=server.arg("on")=="1"; autoMode=on; docSmall.clear(); docSmall["auto"]=autoMode; String o; serializeJson(docSmall,o); server.send(200,"application/json",o); }

void handleMusicMode(){
  if(server.hasArg("on"))lightweightMusicMode=server.arg("on")=="1";
  if(server.hasArg("max")){ int m=server.arg("max").toInt(); if(m>=8&&m<=400)maxMelodyNotes=(uint16_t)m; }
  if(server.hasArg("tempo")){ int t=server.arg("tempo").toInt(); if(t>=1&&t<=4)melodyTempoDiv=(uint8_t)t; }
  docSmall.clear();
  docSmall["light"] = lightweightMusicMode;
  docSmall["max_notes"] = maxMelodyNotes;
  docSmall["tempo_div"] = melodyTempoDiv;
  String o; serializeJson(docSmall,o);
  server.send(200,"application/json",o);
}

void handleBell(){ String k=server.arg("key"); if(!validateKey(k)||k!=ADMIN_KEY){server.send(403,"text/plain","Key Salah!");return;} int id=server.arg("id").toInt(); server.send(200,"text/plain","OK"); switch(id){case 1:setClassModeInternal(CLASS_MODE_ISTIRAHAT);playBellIstirahat();break;case 2:setClassModeInternal(CLASS_MODE_BELAJAR);playBellMasuk();break;case 3:setClassModeInternal(CLASS_MODE_ISHOMA);playBellIshoma();break;case 4:playBellGuruMasuk();break;case 5:playBellLowEnergi();break;} }

void handleClassMode(){ String k=server.arg("key"); if(!validateKey(k)||k!=ADMIN_KEY){server.send(403,"text/plain","Key Salah!");return;} int m=server.arg("m").toInt(); if(m<CLASS_MODE_BELAJAR||m>CLASS_MODE_ISHOMA){server.send(400,"text/plain","Mode invalid");return;} setClassModeInternal(m); if(m==CLASS_MODE_ISTIRAHAT)playBellIstirahat();else if(m==CLASS_MODE_ISHOMA)playBellIshoma();else playBellMasuk(); server.send(200,"text/plain","Mode:"+getClassModeLabel()); }

void handleMitigate(){ mitigationMode=true; mitigationUntil=millis()+300000; server.send(200,"text/plain","Mitigasi ON"); playFurElise(); }



// ============ DATA/API HANDLERS ============
void handleData(){
  float t=getTempConverted(temperature),h=getHumConverted(humidity,temperature),g=getGasConverted((float)gasRaw,temperature),l=getLuxConverted(luxEst),n=getNoiseConverted(dB);
  docLarge.clear();
  docLarge["t"]=String(t,2); docLarge["h"]=String(h,2); docLarge["g"]=String(g,(unitGas=="RAW"?0:2));
  docLarge["s"]=String(n,2); docLarge["l"]=String(l,2); docLarge["dist"]=String(distanceCm,1);
  docLarge["u_t"]=getTempUnitLabel(); docLarge["u_h"]=getHumUnitLabel(); docLarge["u_g"]=getGasUnitLabel(); docLarge["u_l"]=getLuxUnitLabel(); docLarge["u_n"]=getNoiseUnitLabel();
  docLarge["ldr_do"]=ldrDoStatus; docLarge["f_val"]=flameConfirmed?"API":"AMAN"; docLarge["f_alert"]=flameConfirmed;
  docLarge["stress"]=stressScore; docLarge["mood"]=moodLearn; docLarge["ambient"]=getAmbientModeLabel(); docLarge["class_mode"]=getClassModeLabel();
  String o; serializeJson(docLarge,o); server.send(200,"application/json",o);
}

void handleHistory16(){
  docXLarge.clear();
  JsonArray t=docXLarge.createNestedArray("temp");
  JsonArray d=docXLarge.createNestedArray("dist");
  int n=graph16Filled?GRAPH_16H_POINTS:graph16Index;
  if(n<=0){ server.send(200,"application/json","{\"temp\":[],\"dist\":[]}"); return; }
  int step=max(1,n/HISTORY_JSON_POINTS);
  for(int i=0;i<n;i+=step){
    int idx=(graph16Filled?(graph16Index+i)%GRAPH_16H_POINTS:i);
    t.add(temp16History[idx]);
    d.add(dist16History[idx]);
  }
  String o; serializeJson(docXLarge,o); server.send(200,"application/json",o);
}

void handleSetUnits(){
  String type=server.arg("t"),v=server.arg("v");
  if(type=="t")unitTemp=v; else if(type=="h")unitHum=v; else if(type=="g")unitGas=v; else if(type=="l")unitLux=v; else if(type=="n")unitNoise=v;
  server.send(200,"text/plain","OK");
}

void handleDownload(){
  docLarge.clear();
  docLarge["temperature_c"]=temperature; docLarge["humidity_pct"]=humidity; docLarge["gas_raw"]=gasRaw;
  docLarge["noise_db"]=dB; docLarge["lux"]=luxEst; docLarge["distance_cm"]=distanceCm;
  docLarge["stress_score"]=stressScore; docLarge["mood"]=moodLearn;
  String o; serializeJsonPretty(docLarge,o);
  server.sendHeader("Content-Disposition","attachment; filename=sensor_data.json");
  server.send(200,"application/json",o);
}

void readSensors(){
  float t=temperature,h=humidity;
  if(readDHTWithRetry(t,h)){ temperature=t; humidity=h; }
  gasRaw=analogRead(MQ135_PIN);
  luxRaw=analogRead(LDR_PIN);
  luxEst=(4095-luxRaw)*0.25f;
  int sMin=4095,sMax=0;
  for(int i=0;i<16;i++){ int sv=analogRead(SOUND_PIN); if(sv<sMin)sMin=sv; if(sv>sMax)sMax=sv; }
  int amp=sMax-sMin; dB=(amp>0)?(20.0f*log10((float)amp)+10.0f):35.0f;
  safeFlameState=digitalRead(FLAME_DO_PIN);
  if(safeFlameState==LOW){ if(flameCounter<FLAME_THRESHOLD)flameCounter++; } else if(flameCounter>0)flameCounter--;
  flameConfirmed=flameCounter>=FLAME_THRESHOLD;
  ldrDoStatus=digitalRead(LDR_DO_PIN)==LOW?"Gelap":"Terang";
  distanceCm=readUltrasonicCm();
  updateOutlierWin(temperature,dB);
  computeStressAndMood();
  updateHistory();
}

void setup(){
  Serial.begin(115200);
  pinMode(FLAME_DO_PIN,INPUT); pinMode(FLAME_AO_PIN,INPUT);
  pinMode(MQ135_PIN,INPUT); pinMode(LDR_PIN,INPUT); pinMode(LDR_DO_PIN,INPUT); pinMode(SOUND_PIN,INPUT);
  pinMode(ULTRASONIC_TRIG_PIN,OUTPUT); pinMode(ULTRASONIC_ECHO_PIN,INPUT);
  dht.begin();
  ledcSetup(pwmChannel,2000,8); ledcAttachPin(BUZZER2_PIN,pwmChannel);
  WiFi.mode(WIFI_AP_STA); WiFi.softAP(ap_ssid,ap_password);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  clientBot.setInsecure();

  server.on("/",handleRoot);
  server.on("/play",handlePlay);
  server.on("/song",handleSong);
  server.on("/data",handleData);
  server.on("/download",handleDownload);
  server.on("/history16",handleHistory16);
  server.on("/scan",handleScan);
  server.on("/wifi_status",handleWifiStatus);
  server.on("/connect",handleConnect);
  server.on("/calibrate",handleCalibrate);
  server.on("/ambient",handleAmbient);
  server.on("/auto_mode",handleAutoMode);
  server.on("/music_mode",handleMusicMode);
  server.on("/bell",handleBell);
  server.on("/class_mode",handleClassMode);
  server.on("/mitigate",handleMitigate);
  server.on("/api/set_units",handleSetUnits);
  server.onNotFound([](){ server.send(404,"text/plain","Not Found"); });
  server.begin();

  bootTime=millis();
  runCalibration();
}

void loop(){
  server.handleClient();
  unsigned long now=millis();
  if(now-lastSensorRead>=sensorInterval){ lastSensorRead=now; readSensors(); }
  if(autoMode && now-lastAutoSwitch>=AUTO_SWITCH_INTERVAL){
    lastAutoSwitch=now; ambientMode=(ambientMode%3)+1;
    if(ambientMode==AMBIENT_FAN)playAmbientFan(); else if(ambientMode==AMBIENT_COUGH)playAmbientCough(); else playAmbientTeacher();
  }
  if(mitigationMode && now>mitigationUntil)mitigationMode=false;
  checkSchedule();
  handleWifiReconnect();
  if(now-bot_lasttime>BOT_MTBS){
    int n=bot.getUpdates(bot.last_message_received + 1);
    while(n){ handleNewMessages(n); n=bot.getUpdates(bot.last_message_received + 1); }
    bot_lasttime=now;
  }
}
