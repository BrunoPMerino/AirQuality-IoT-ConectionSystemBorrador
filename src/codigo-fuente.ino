/*
 * ============================================================
 *  Monitor de Calidad del Aire — ESP32
 *  Universidad de La Sabana — IoT 2026-1  Challenge #2
 *
 *  Hardware: ESP32 DevKit, MQ135, PMS5003, BME280,
 *            LCD I2C 16x2, LED RGB, Buzzer activo
 * ============================================================
 *
 *  MAPA DE PINES
 * ┌─────────────┬──────────────┬──────────────────────────────┐
 * │ Componente  │ Pin ESP32    │ Notas                        │
 * ├─────────────┼──────────────┼──────────────────────────────┤
 * │ BME280 SDA  │ GPIO 21      │ Bus I2C hardware             │
 * │ BME280 SCL  │ GPIO 22      │ Bus I2C hardware             │
 * │ LCD SDA     │ GPIO 21      │ Bus I2C compartido           │
 * │ LCD SCL     │ GPIO 22      │ Bus I2C compartido           │
 * │ PMS5003 TX  │ GPIO 16 (RX2)│ Serial2 hardware             │
 * │ PMS5003 RX  │ GPIO 17 (TX2)│ No conectado físicamente     │
 * │ PMS5003 SET │ 3.3V         │ Siempre activo               │
 * │ PMS5003 RST │ 3.3V         │ Siempre activo               │
 * │ MQ135 AOUT  │ GPIO 34      │ Solo entrada, sin interferencia│
 * │ LED Rojo    │ GPIO 25 (PWM)│ LED RGB cátodo común         │
 * │ LED Verde   │ GPIO 26 (PWM)│                              │
 * │ LED Azul    │ GPIO 27 (PWM)│                              │
 * │ Buzzer (+)  │ GPIO 14      │ Buzzer activo                │
 * └─────────────┴──────────────┴──────────────────────────────┘
 *
 *  LIBRERÍAS NECESARIAS
 *  - Adafruit BME280 Library  (+ Adafruit Unified Sensor)
 *  - LiquidCrystal I2C (Frank de Brabander)
 *  - ESP32 board support (Espressif)
 *
 *  ARQUITECTURA DE HILOS (FreeRTOS)
 *  - Tarea sensores  (Core 0): Lee BME280, MQ135, PMS5003
 *                               cada 2 s. Calcula estado fusionado.
 *                               Guarda en buffer histórico.
 *  - Tarea servidor  (Core 0): Atiende peticiones HTTP del dashboard.
 *  - Loop principal  (Core 1): LCD, LED RGB, Buzzer.
 *
 *  SEGURIDAD
 *  - Autenticación HTTP básica por usuario/contraseña
 *  - Acceso restringido por red: solo dispositivos en el WiFi local
 *  - Tres niveles de usuario: admin, inspector (control total),
 *    publico (solo lectura)
 *
 *  ENDPOINTS DEL SERVIDOR
 *  GET /            → Dashboard HTML completo
 *  GET /data        → JSON con valores actuales
 *  GET /history     → JSON con histórico (hasta 2 horas)
 *  GET /alarm/off   → Desactivar buzzer (requiere permiso)
 *  GET /alerts      → JSON con historial de últimas alertas (fecha/hora)
 *
 *  HISTÓRICO
 *  Buffer circular de 1800 puntos (1 cada 2 s = 1 hora de datos)
 *  Almacenado en RAM del ESP32 (320 KB disponibles)
 * ============================================================
 */

// ── Librerías ───────────────────────────────────────────────
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <WebServer.h>

// ── Credenciales WiFi ───────────────────────────────────────
// Cambiar por los datos de la red de la alcaldía
const char* WIFI_SSID     = "AndroidAP00BF";
const char* WIFI_PASSWORD = "xfod3809";

// ── Pines ───────────────────────────────────────────────────
#define MQ135_PIN    34   // ADC solo entrada, no interfiere con WiFi
#define LED_R_PIN    25
#define LED_G_PIN    26
#define LED_B_PIN    27
#define BUZZER_PIN   14
#define PMS_RX_PIN   16   // Serial2 RX — conectar al TX del PMS5003
#define PMS_TX_PIN   17   // Serial2 TX — no conectado físicamente

// ── Configuración PWM (ESP32 Arduino Core v3.x) ─────────────
// Los canales LEDC numerados ya no existen; se usa el pin directamente.
#define PWM_FREQ 5000
#define PWM_RES  8        // 8 bits → 0–255

// ── Direcciones I2C ─────────────────────────────────────────
#define LCD_I2C_ADDR  0x27   // Cambiar a 0x3F si no inicia
#define BME_I2C_ADDR  0x76   // Cambiar a 0x77 si SDO en VCC

// ── Objetos hardware ────────────────────────────────────────
LiquidCrystal_I2C lcd(LCD_I2C_ADDR, 16, 2);
Adafruit_BME280   bme;
WebServer         server(80);

// ── Estados de calidad del aire ─────────────────────────────
enum AirState : uint8_t { AIR_OK = 0, AIR_ADV = 1, AIR_PEL = 2 };
const char* STATE_LABEL[] = { "OK ", "ADV", "PEL" };
const char* STATE_JSON[]  = { "OK",  "ADV", "PEL" };

// ── Usuarios autorizados ────────────────────────────────────
struct Usuario {
  const char* nombre;
  const char* contrasena;
  bool        puedeDesactivarAlarma;
};

const Usuario USUARIOS[] = {
  { "alcaldia",  "clave2026", true  },   // Control total
  { "inspector", "aire2026",  true  },   // Control total
  { "publico",   "ver2026",   false }    // Solo lectura
};
const int NUM_USUARIOS = 3;

// ── Estructura PMS5003 ──────────────────────────────────────
struct PMS5003Data {
  uint16_t pm1_0_atm;
  uint16_t pm2_5_atm;
  uint16_t pm10_atm;
};

// ── Punto de datos para el histórico ────────────────────────
struct DataPoint {
  float    temp;
  float    hum;
  float    pres;
  int      rawMQ;
  uint16_t pm25;
  uint8_t  estado;
  uint32_t ts;       // millis() al momento de la lectura
};

// ── Buffer histórico circular ───────────────────────────────
// 1800 puntos × 1 lectura/2s = 1 hora de histórico
#define HIST_SIZE 1800
DataPoint historial[HIST_SIZE];
int  histIdx   = 0;
int  histCount = 0;

// ── Historial de alertas ────────────────────────────────────
// Guarda las últimas 50 alertas con timestamp y nivel
struct AlertEvent {
  uint8_t  nivel;     // AIR_ADV=1, AIR_PEL=2
  uint32_t ts;        // millis() al momento de la alerta
};
#define ALERT_HIST_SIZE 50
AlertEvent alertHistorial[ALERT_HIST_SIZE];
int  alertHistIdx   = 0;
int  alertHistCount = 0;
AirState g_lastAlertState = AIR_OK;  // Para detectar transición nueva alerta

// ── Variables globales de medición ──────────────────────────
volatile float    g_temperature = 0.0f;
volatile float    g_humidity    = 0.0f;
volatile float    g_pressure    = 0.0f;
volatile int      g_rawMQ       = 1023;
volatile uint16_t g_pm2_5       = 0;
volatile AirState g_airState    = AIR_OK;
volatile bool     alarmaDesactivada = false;
// alarmaDesactivada se resetea solo cuando el estado vuelve a AIR_OK
// y luego sube de nuevo. El usuario puede activar/desactivar libremente.

// ── Mutex para acceso seguro entre hilos ────────────────────
SemaphoreHandle_t xDataMutex;

// ── Temporización LCD ───────────────────────────────────────
unsigned long lastScreenSwitch = 0;
unsigned long lastLCDUpdate    = 0;
unsigned long lastBuzzerToggle = 0;

const unsigned long SCREEN_INTERVAL = 5000;
const unsigned long LCD_UPDATE_MS   = 500;
const unsigned long BUZZER_INTERVAL = 500;

int  g_currentScreen = 0;
bool g_buzzerState   = false;

// ── Buffer PMS5003 ──────────────────────────────────────────
#define PMS_FRAME_LEN 32
uint8_t pmsBuffer[PMS_FRAME_LEN];
uint8_t pmsIdx = 0;

// ════════════════════════════════════════════════════════════
//  HTML DEL DASHBOARD
//  Guardado en flash (PROGMEM) para no consumir RAM
// ════════════════════════════════════════════════════════════
const char DASHBOARD_HTML[] PROGMEM = R"rawHTML(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Monitor Calidad del Aire</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: 'Segoe UI', sans-serif; background: #0f172a; color: #e2e8f0; }
  header { background: #1e293b; padding: 16px 24px; display: flex;
           align-items: center; justify-content: space-between;
           border-bottom: 2px solid #334155; }
  header h1 { font-size: 1.2rem; color: #38bdf8; }
  header span { font-size: 0.85rem; color: #94a3b8; }
  .cards { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
           gap: 12px; padding: 20px; }
  .card { background: #1e293b; border-radius: 12px; padding: 16px;
          text-align: center; border: 1px solid #334155; }
  .card .label { font-size: 0.75rem; color: #94a3b8; text-transform: uppercase;
                 letter-spacing: 0.05em; margin-bottom: 8px; }
  .card .value { font-size: 1.8rem; font-weight: 700; color: #f1f5f9; }
  .card .unit  { font-size: 0.75rem; color: #64748b; margin-top: 4px; }
  .estado-ok  { color: #4ade80 !important; }
  .estado-adv { color: #facc15 !important; }
  .estado-pel { color: #f87171 !important; }
  .charts { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 16px; padding: 0 20px 20px; }
  .chart-box { background: #1e293b; border-radius: 12px; padding: 16px;
               border: 1px solid #334155; }
  .chart-box h3 { font-size: 0.85rem; color: #94a3b8; margin-bottom: 12px; }
  .alarm-panel { margin: 0 20px 20px; background: #1e293b; border-radius: 12px;
                 padding: 16px; border: 1px solid #334155;
                 display: flex; align-items: center; justify-content: space-between; }
  .alarm-status { font-size: 0.9rem; }
  #btnAlarm { background: #ef4444; color: white; border: none; border-radius: 8px;
              padding: 10px 20px; cursor: pointer; font-size: 0.9rem;
              transition: background 0.2s; }
  #btnAlarm:hover { background: #dc2626; }
  #btnAlarm:disabled { background: #475569; cursor: not-allowed; }
  .badge { display: inline-block; padding: 4px 12px; border-radius: 99px;
           font-size: 0.8rem; font-weight: 600; }
  .badge-ok  { background: #14532d; color: #4ade80; }
  .badge-adv { background: #713f12; color: #facc15; }
  .badge-pel { background: #7f1d1d; color: #f87171; }
  .alert-history { margin: 0 20px 20px; background: #1e293b; border-radius: 12px;
                   padding: 16px; border: 1px solid #334155; }
  .alert-history h3 { font-size: 0.85rem; color: #94a3b8; margin-bottom: 12px; }
  #alertTable { width: 100%; border-collapse: collapse; font-size: 0.85rem; }
  #alertTable th { color: #64748b; font-weight: 600; text-align: left;
                   padding: 6px 8px; border-bottom: 1px solid #334155; }
  #alertTable td { padding: 6px 8px; border-bottom: 1px solid #1e293b; color: #e2e8f0; }
  #alertTable tr:last-child td { border-bottom: none; }
  .nivel-adv { color: #facc15; font-weight: 700; }
  .nivel-pel { color: #f87171; font-weight: 700; }
  #noAlerts { color: #475569; font-size: 0.85rem; text-align: center; padding: 12px 0; }
  #btnReactivar { background: #22c55e; color: white; border: none; border-radius: 8px;
                  padding: 10px 20px; cursor: pointer; font-size: 0.9rem;
                  transition: background 0.2s; display: none; margin-left: 10px; }
  #btnReactivar:hover { background: #16a34a; }
  footer { text-align: center; padding: 12px; font-size: 0.75rem; color: #475569; }
</style>
</head>
<body>

<header>
  <h1>🌿 Monitor de Calidad del Aire — Sabana Centro</h1>
  <span id="lastUpdate">Actualizando...</span>
</header>

<div class="cards">
  <div class="card">
    <div class="label">Temperatura</div>
    <div class="value" id="temp">--</div>
    <div class="unit">°C</div>
  </div>
  <div class="card">
    <div class="label">Humedad</div>
    <div class="value" id="hum">--</div>
    <div class="unit">%</div>
  </div>
  <div class="card">
    <div class="label">Presión</div>
    <div class="value" id="pres">--</div>
    <div class="unit">hPa</div>
  </div>
  <div class="card">
    <div class="label">PM2.5</div>
    <div class="value" id="pm25">--</div>
    <div class="unit">µg/m³</div>
  </div>
  <div class="card">
    <div class="label">Índice Gas</div>
    <div class="value" id="gas">--</div>
    <div class="unit">RAW ADC</div>
  </div>
  <div class="card">
    <div class="label">Estado Aire</div>
    <div class="value" id="estado" style="font-size:1.4rem">--</div>
    <div class="unit" id="estadoDesc">--</div>
  </div>
</div>

<div class="alarm-panel">
  <div class="alarm-status">
    🔔 Alarma: <span id="alarmStatus">Activa</span>
  </div>
  <div>
    <button id="btnAlarm" onclick="desactivarAlarma()">🔕 Desactivar Alarma</button>
    <button id="btnReactivar" onclick="reactivarAlarma()">🔔 Reactivar Alarma</button>
  </div>
</div>

<div class="alert-history">
  <h3>🚨 Historial de Alertas Recientes</h3>
  <div id="noAlerts">Sin alertas registradas</div>
  <table id="alertTable" style="display:none">
    <thead>
      <tr><th>#</th><th>Hora (desde inicio)</th><th>Nivel</th></tr>
    </thead>
    <tbody id="alertBody"></tbody>
  </table>
</div>

<div class="charts">
  <div class="chart-box">
    <h3>📈 PM2.5 — Última hora (µg/m³)</h3>
    <canvas id="chartPM"></canvas>
  </div>
  <div class="chart-box">
    <h3>🌡️ Temperatura — Última hora (°C)</h3>
    <canvas id="chartTemp"></canvas>
  </div>
  <div class="chart-box">
    <h3>💧 Humedad — Última hora (%)</h3>
    <canvas id="chartHum"></canvas>
  </div>
  <div class="chart-box">
    <h3>🧪 Índice de Gas — Última hora (RAW)</h3>
    <canvas id="chartGas"></canvas>
  </div>
</div>

<footer>Monitor IoT — Universidad de La Sabana 2026</footer>

<script>
// ── Configuración de gráficas ────────────────────────────────
// MAX_VISIBLE: puntos visibles en pantalla (60 = 2 min a 2 s/lectura)
// Los puntos iniciales son null para que la línea empiece al final del eje.
const MAX_VISIBLE = 60;

function makeNullArray(n) {
  return Array(n).fill(null);
}

function makeLabels(n) {
  // Etiquetas vacías; se sobrescriben en addPoint
  return Array(n).fill('');
}

const chartOpts = (label, color) => ({
  type: 'line',
  data: {
    labels: makeLabels(MAX_VISIBLE),
    datasets: [{
      label,
      data: makeNullArray(MAX_VISIBLE),
      borderColor: color,
      backgroundColor: color + '22',
      borderWidth: 2,
      pointRadius: 0,
      fill: true,
      tension: 0.3,
      spanGaps: false   // No unir segmentos a través de null
    }]
  },
  options: {
    animation: false,
    plugins: { legend: { display: false } },
    scales: {
      x: { ticks: { color: '#64748b', maxTicksLimit: 6 },
           grid: { color: '#1e293b' } },
      y: { ticks: { color: '#64748b' }, grid: { color: '#334155' } }
    }
  }
});

const chartPM   = new Chart(document.getElementById('chartPM'),   chartOpts('PM2.5', '#f87171'));
const chartTemp = new Chart(document.getElementById('chartTemp'), chartOpts('Temp',  '#38bdf8'));
const chartHum  = new Chart(document.getElementById('chartHum'),  chartOpts('Hum',   '#34d399'));
const chartGas  = new Chart(document.getElementById('chartGas'),  chartOpts('Gas',   '#a78bfa'));

// addPoint: desplaza el array circular y añade el nuevo punto al final
function addPoint(chart, label, value) {
  const labels = chart.data.labels;
  const data   = chart.data.datasets[0].data;
  labels.shift(); labels.push(label);
  data.shift();   data.push(value);
  chart.update('none');
}

// ── Actualizar tarjetas de valores actuales ──────────────────
function actualizarDashboard(d) {
  document.getElementById('temp').textContent  = d.temperatura.toFixed(1);
  document.getElementById('hum').textContent   = Math.round(d.humedad);
  document.getElementById('pres').textContent  = Math.round(d.presion);
  document.getElementById('pm25').textContent  = d.pm25;
  document.getElementById('gas').textContent   = d.gas;

  const estadoEl   = document.getElementById('estado');
  const descEl     = document.getElementById('estadoDesc');
  const alarmBtn   = document.getElementById('btnAlarm');
  const reacBtn    = document.getElementById('btnReactivar');
  const alarmSt    = document.getElementById('alarmStatus');

  estadoEl.className = 'value estado-' + d.estado.toLowerCase();
  estadoEl.textContent = d.estado;

  const descs = { OK: 'Aire en buen estado', ADV: 'Precaución', PEL: '¡Peligro!' };
  descEl.textContent = descs[d.estado] || '';

  // Estado del botón de alarma y reactivar
  if (d.alarmaDesactivada) {
    alarmBtn.disabled        = true;
    alarmBtn.textContent     = '✅ Alarma desactivada';
    alarmSt.textContent      = 'Desactivada manualmente';
    reacBtn.style.display    = (d.estado !== 'OK') ? 'inline-block' : 'none';
  } else {
    alarmBtn.disabled        = (d.estado === 'OK');
    alarmBtn.textContent     = '🔕 Desactivar Alarma';
    alarmSt.textContent      = d.estado === 'OK' ? 'Sin alertas activas' : 'Activa';
    reacBtn.style.display    = 'none';
  }

  // Timestamp legible
  const now = new Date();
  const hh  = String(now.getHours()).padStart(2,'0');
  const mm  = String(now.getMinutes()).padStart(2,'0');
  const ss  = String(now.getSeconds()).padStart(2,'0');
  const label = hh + ':' + mm + ':' + ss;
  document.getElementById('lastUpdate').textContent = 'Actualizado: ' + label;

  addPoint(chartPM,   label, d.pm25);
  addPoint(chartTemp, label, d.temperatura);
  addPoint(chartHum,  label, d.humedad);
  addPoint(chartGas,  label, d.gas);
}

// ── Cargar histórico inicial al abrir el dashboard ───────────
// Solo se carga una vez. Los puntos se alinean al lado DERECHO
// del eje completando con null por la izquierda.
function cargarHistorico() {
  fetch('/history')
    .then(r => r.json())
    .then(h => {
      const n = h.labels.length;
      const pad = MAX_VISIBLE - n;

      // Rellenar con null+etiquetas vacías a la izquierda
      const labels = Array(pad > 0 ? pad : 0).fill('').concat(h.labels.slice(-(MAX_VISIBLE)));
      const pm25   = Array(pad > 0 ? pad : 0).fill(null).concat(h.pm25.slice(-(MAX_VISIBLE)));
      const temp   = Array(pad > 0 ? pad : 0).fill(null).concat(h.temp.slice(-(MAX_VISIBLE)));
      const hum    = Array(pad > 0 ? pad : 0).fill(null).concat(h.hum.slice(-(MAX_VISIBLE)));
      const gas    = Array(pad > 0 ? pad : 0).fill(null).concat(h.gas.slice(-(MAX_VISIBLE)));

      [chartPM, chartTemp, chartHum, chartGas].forEach((c, i) => {
        c.data.labels = [...labels];
        c.data.datasets[0].data = [pm25, temp, hum, gas][i];
        c.update();
      });
    })
    .catch(e => console.warn('Histórico no disponible:', e));
}

// ── Cargar historial de alertas ──────────────────────────────
function cargarAlertas() {
  fetch('/alerts')
    .then(r => r.json())
    .then(list => {
      const tbody    = document.getElementById('alertBody');
      const noAlerts = document.getElementById('noAlerts');
      const table    = document.getElementById('alertTable');

      if (!list || list.length === 0) {
        noAlerts.style.display = 'block';
        table.style.display    = 'none';
        return;
      }
      noAlerts.style.display = 'none';
      table.style.display    = 'table';

      tbody.innerHTML = '';
      // Mostrar de más reciente a más antiguo
      const reversed = list.slice().reverse();
      reversed.forEach((a, i) => {
        const clase = a.nivel === 'PEL' ? 'nivel-pel' : 'nivel-adv';
        const emoji = a.nivel === 'PEL' ? '🔴' : '🟡';
        const fila  = `<tr>
          <td>${reversed.length - i}</td>
          <td>${a.ts}</td>
          <td class="${clase}">${emoji} ${a.nivel}</td>
        </tr>`;
        tbody.insertAdjacentHTML('beforeend', fila);
      });
    })
    .catch(e => console.warn('Alertas no disponibles:', e));
}

// ── Desactivar alarma ────────────────────────────────────────
function desactivarAlarma() {
  fetch('/alarm/off')
    .then(r => r.json())
    .then(d => {
      if (d.status === 'ok') {
        document.getElementById('btnAlarm').disabled = true;
        document.getElementById('btnAlarm').textContent = '✅ Alarma desactivada';
        document.getElementById('alarmStatus').textContent = 'Desactivada manualmente';
        document.getElementById('btnReactivar').style.display = 'inline-block';
      } else {
        alert('Sin permisos para desactivar la alarma.');
      }
    })
    .catch(() => alert('Error al contactar el servidor.'));
}

// ── Reactivar alarma ─────────────────────────────────────────
function reactivarAlarma() {
  fetch('/alarm/on')
    .then(r => r.json())
    .then(d => {
      if (d.status === 'ok') {
        document.getElementById('btnAlarm').disabled    = false;
        document.getElementById('btnAlarm').textContent = '🔕 Desactivar Alarma';
        document.getElementById('btnReactivar').style.display = 'none';
        document.getElementById('alarmStatus').textContent = 'Activa';
      } else {
        alert('Sin permisos para reactivar la alarma.');
      }
    })
    .catch(() => alert('Error al contactar el servidor.'));
}

// ── Polling de datos cada 6 s y alertas cada 15 s ───────────
cargarHistorico();
cargarAlertas();

setInterval(() => {
  fetch('/data')
    .then(r => r.json())
    .then(d => actualizarDashboard(d))
    .catch(e => console.warn('Error al obtener datos:', e));
}, 6000);

setInterval(cargarAlertas, 15000);
</script>
</body>
</html>
)rawHTML";

// ════════════════════════════════════════════════════════════
//  LECTURA PMS5003 — no bloqueante, byte a byte
// ════════════════════════════════════════════════════════════
bool parsePMS5003(PMS5003Data &data) {
  while (Serial2.available()) {
    uint8_t c = Serial2.read();

    if (pmsIdx == 0) {
      if (c != 0x42) continue;
    } else if (pmsIdx == 1) {
      if (c != 0x4D) { pmsIdx = 0; continue; }
    }

    pmsBuffer[pmsIdx++] = c;

    if (pmsIdx == PMS_FRAME_LEN) {
      pmsIdx = 0;

      uint16_t sum = 0;
      for (uint8_t i = 0; i < 30; i++) sum += pmsBuffer[i];
      uint16_t rxCRC = ((uint16_t)pmsBuffer[30] << 8) | pmsBuffer[31];
      if (sum != rxCRC) return false;

      data.pm1_0_atm = ((uint16_t)pmsBuffer[10] << 8) | pmsBuffer[11];
      data.pm2_5_atm = ((uint16_t)pmsBuffer[12] << 8) | pmsBuffer[13];
      data.pm10_atm  = ((uint16_t)pmsBuffer[14] << 8) | pmsBuffer[15];
      return true;
    }
  }
  return false;
}

// ════════════════════════════════════════════════════════════
//  LECTURA MQ135 — Índice RAW (promedio 8 muestras)
//  Valor alto (~750–900) = aire limpio
//  Valor bajo (< 580)    = presencia de gas
// ════════════════════════════════════════════════════════════
int readMQ135_RAW() {
  long suma = 0;
  for (int i = 0; i < 8; i++) {
    suma += analogRead(MQ135_PIN);
    vTaskDelay(pdMS_TO_TICKS(2));
  }
  return (int)(suma / 8);
}

// ════════════════════════════════════════════════════════════
//  FUSIÓN PONDERADA DE CALIDAD DEL AIRE
//
//  Pesos: PM2.5(40%) + Gas(35%) + Temp(15%) + Hum(10%)
//  Score: < 0.25 = OK | 0.25–0.60 = ADV | > 0.60 = PEL
// ════════════════════════════════════════════════════════════
AirState computeAirState(float temp, float hum, int rawMQ, uint16_t pm25) {

  float sPM = (pm25 < 25) ? 0.0f :
              (pm25 < 70) ? 0.5f : 1.0f;

  // RAW directo: valor alto = más gas detectado
  float sGas = (rawMQ < 1600) ? 0.0f :
               (rawMQ < 2000) ? 0.5f : 1.0f;

  float sTemp;
  if      (temp >= 16.0f && temp <= 28.0f) sTemp = 0.0f;
  else if (temp >=  8.0f && temp <  16.0f) sTemp = 0.5f;
  else if (temp >  28.0f && temp <= 36.0f) sTemp = 0.5f;
  else                                      sTemp = 1.0f;

  float sHum;
  if      (hum >= 30.0f && hum <= 70.0f)  sHum = 0.0f;
  else if (hum >= 20.0f && hum <  30.0f)  sHum = 0.5f;
  else if (hum >  70.0f && hum <= 80.0f)  sHum = 0.5f;
  else                                      sHum = 1.0f;

  float score = sPM   * 0.40f
              + sGas  * 0.35f
              + sTemp * 0.15f
              + sHum  * 0.10f;

  if      (score < 0.25f) return AIR_OK;
  else if (score < 0.60f) return AIR_ADV;
  else                    return AIR_PEL;
}

// ════════════════════════════════════════════════════════════
//  LED RGB — PWM nativo ESP32 (LEDC)
// ════════════════════════════════════════════════════════════
void setRGB(uint8_t r, uint8_t g, uint8_t b) {
  // En ESP32 Arduino Core v3.x, ledcWrite recibe el pin directamente
  ledcWrite(LED_R_PIN, r);
  ledcWrite(LED_G_PIN, g);
  ledcWrite(LED_B_PIN, b);
}

void updateLED(AirState state) {
  switch (state) {
    case AIR_OK:  setRGB(0,   255, 0);  break;
    case AIR_ADV: setRGB(255, 180, 0);  break;
    case AIR_PEL: setRGB(255, 0,   0);  break;
  }
}

// ════════════════════════════════════════════════════════════
//  BUZZER
//  - AIR_OK  : silencio + reset de alarmaDesactivada
//              (próxima alerta sonará desde cero)
//  - AIR_ADV : pitido intermitente cada 500 ms
//  - AIR_PEL : tono continuo 1500 Hz
//  El usuario puede activar/desactivar el buzzer libremente
//  desde el dashboard. Solo se reactiva automáticamente cuando
//  el estado pasa por OK y luego vuelve a subir.
// ════════════════════════════════════════════════════════════
void updateBuzzer(AirState state) {
  unsigned long now = millis();

  // Volvió a OK → silencio total y reset del flag
  if (state == AIR_OK) {
    noTone(BUZZER_PIN);
    g_buzzerState     = false;
    alarmaDesactivada = false;   // Reset: próxima alerta sonará sola
    return;
  }

  // Usuario desactivó el buzzer → silencio hasta que él lo reactive
  // o el sistema pase por OK
  if (alarmaDesactivada) {
    noTone(BUZZER_PIN);
    return;
  }

  if (state == AIR_PEL) {
    tone(BUZZER_PIN, 1500);
    return;
  }

  // ADV: parpadeo
  if (now - lastBuzzerToggle >= BUZZER_INTERVAL) {
    lastBuzzerToggle = now;
    g_buzzerState = !g_buzzerState;
    if (g_buzzerState) tone(BUZZER_PIN, 900);
    else               noTone(BUZZER_PIN);
  }
}

// ════════════════════════════════════════════════════════════
//  LCD — Pantalla 1 y Pantalla 2
// ════════════════════════════════════════════════════════════
void showScreen1() {
  float t, h, p;
  AirState st;
  if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(10))) {
    t  = g_temperature;
    h  = g_humidity;
    p  = g_pressure;
    st = g_airState;
    xSemaphoreGive(xDataMutex);
  } else return;

  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(t, 1);
  lcd.print("C H:");
  if (h < 100.0f) lcd.print(' ');
  lcd.print((int)h);
  lcd.print('%');
  lcd.print(' ');

  lcd.setCursor(0, 1);
  lcd.print("P:");
  if (p < 1000.0f) lcd.print(' ');
  lcd.print((int)p);
  lcd.print("hPa ");
  lcd.print(STATE_LABEL[st]);
}

void showScreen2() {
  int      raw;
  uint16_t pm;
  AirState st;
  if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(10))) {
    raw = g_rawMQ;
    pm  = g_pm2_5;
    st  = g_airState;
    xSemaphoreGive(xDataMutex);
  } else return;

  lcd.setCursor(0, 0);
  lcd.print("PM2.5:");
  if (pm < 100) lcd.print(' ');
  if (pm <  10) lcd.print(' ');
  lcd.print(pm);
  lcd.print("ug/m3");

  lcd.setCursor(0, 1);
  lcd.print("GAS:");
  if (raw < 1000) lcd.print(' ');
  if (raw <  100) lcd.print(' ');
  if (raw <   10) lcd.print(' ');
  lcd.print(raw);
  lcd.print("  ");
  lcd.print(STATE_LABEL[st]);
}

void updateLCD() {
  if (g_currentScreen == 0) showScreen1();
  else                      showScreen2();
}

// ════════════════════════════════════════════════════════════
//  VERIFICACIÓN DE USUARIO
// ════════════════════════════════════════════════════════════
int getUsuarioIdx() {
  for (int i = 0; i < NUM_USUARIOS; i++) {
    if (server.authenticate(USUARIOS[i].nombre, USUARIOS[i].contrasena)) {
      return i;
    }
  }
  return -1;
}

// ════════════════════════════════════════════════════════════
//  HANDLERS DEL SERVIDOR WEB
// ════════════════════════════════════════════════════════════

// GET / → Dashboard HTML
void handleRoot() {
  int idx = getUsuarioIdx();
  if (idx < 0) {
    server.requestAuthentication(BASIC_AUTH, "Monitor Aire",
                                 "Acceso restringido a personal autorizado");
    return;
  }
  server.send_P(200, "text/html", DASHBOARD_HTML);
}

// GET /data → JSON con valores actuales
void handleData() {
  int idx = getUsuarioIdx();
  if (idx < 0) {
    server.requestAuthentication(BASIC_AUTH, "Monitor Aire", "Acceso restringido");
    return;
  }

  float    t, h, p;
  int      raw;
  uint16_t pm;
  AirState st;
  bool     alDes;

  if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(50))) {
    t     = g_temperature;
    h     = g_humidity;
    p     = g_pressure;
    raw   = g_rawMQ;
    pm    = g_pm2_5;
    st    = g_airState;
    alDes = alarmaDesactivada;
    xSemaphoreGive(xDataMutex);
  } else {
    server.send(503, "application/json", "{\"error\":\"Datos no disponibles\"}");
    return;
  }

  String json = "{";
  json += "\"temperatura\":"    + String(t, 1)    + ",";
  json += "\"humedad\":"        + String(h, 1)    + ",";
  json += "\"presion\":"        + String(p, 1)    + ",";
  json += "\"gas\":"            + String(raw)     + ",";
  json += "\"pm25\":"           + String(pm)      + ",";
  json += "\"estado\":\""       + String(STATE_JSON[st]) + "\",";
  json += "\"alarmaDesactivada\":" + String(alDes ? "true" : "false");
  json += "}";

  server.send(200, "application/json", json);
}

// GET /history → JSON con histórico para gráficas
void handleHistory() {
  int idx = getUsuarioIdx();
  if (idx < 0) {
    server.requestAuthentication(BASIC_AUTH, "Monitor Aire", "Acceso restringido");
    return;
  }

  // Construir JSON del histórico (últimos histCount puntos)
  // Para no saturar la respuesta HTTP, limitar a 120 puntos (4 min)
  int maxPuntos = min(histCount, 120);
  int inicio    = (histIdx - maxPuntos + HIST_SIZE) % HIST_SIZE;

  String labels = "[";
  String pm25   = "[";
  String temp   = "[";
  String hum    = "[";
  String gas    = "[";

  for (int i = 0; i < maxPuntos; i++) {
    int pos = (inicio + i) % HIST_SIZE;
    DataPoint &dp = historial[pos];

    // Timestamp en formato MM:SS
    uint32_t seg = dp.ts / 1000;
    uint32_t mm  = (seg / 60) % 60;
    uint32_t ss  = seg % 60;
    char lbl[8];
    snprintf(lbl, sizeof(lbl), "%02lu:%02lu", mm, ss);

    labels += "\""; labels += lbl;    labels += "\"";
    pm25   += String(dp.pm25);
    temp   += String(dp.temp, 1);
    hum    += String(dp.hum,  1);
    gas    += String(dp.rawMQ);

    if (i < maxPuntos - 1) {
      labels += ","; pm25 += ","; temp += ","; hum += ","; gas += ",";
    }
  }

  labels += "]"; pm25 += "]"; temp += "]"; hum += "]"; gas += "]";

  String json = "{\"labels\":" + labels
              + ",\"pm25\":"   + pm25
              + ",\"temp\":"   + temp
              + ",\"hum\":"    + hum
              + ",\"gas\":"    + gas + "}";

  server.send(200, "application/json", json);
}

// GET /alarm/off → Desactivar buzzer
void handleAlarmOff() {
  int idx = getUsuarioIdx();
  if (idx < 0) {
    server.requestAuthentication(BASIC_AUTH, "Monitor Aire", "Acceso restringido");
    return;
  }

  if (!USUARIOS[idx].puedeDesactivarAlarma) {
    server.send(403, "application/json",
                "{\"status\":\"error\",\"mensaje\":\"Sin permisos\"}");
    return;
  }

  if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(50))) {
    alarmaDesactivada = true;
    xSemaphoreGive(xDataMutex);
  }
  noTone(BUZZER_PIN);

  server.send(200, "application/json",
              "{\"status\":\"ok\",\"mensaje\":\"Alarma desactivada\"}");
}

// GET /alarm/on → Reactivar buzzer manualmente
void handleAlarmOn() {
  int idx = getUsuarioIdx();
  if (idx < 0) {
    server.requestAuthentication(BASIC_AUTH, "Monitor Aire", "Acceso restringido");
    return;
  }

  if (!USUARIOS[idx].puedeDesactivarAlarma) {
    server.send(403, "application/json",
                "{\"status\":\"error\",\"mensaje\":\"Sin permisos\"}");
    return;
  }

  if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(50))) {
    alarmaDesactivada = false;
    xSemaphoreGive(xDataMutex);
  }

  server.send(200, "application/json",
              "{\"status\":\"ok\",\"mensaje\":\"Alarma reactivada\"}");
}

// GET /alerts → JSON con historial de alertas
void handleAlerts() {
  int idx = getUsuarioIdx();
  if (idx < 0) {
    server.requestAuthentication(BASIC_AUTH, "Monitor Aire", "Acceso restringido");
    return;
  }

  int total  = alertHistCount;
  int inicio = (alertHistIdx - total + ALERT_HIST_SIZE) % ALERT_HIST_SIZE;

  String json = "[";
  for (int i = 0; i < total; i++) {
    int pos = (inicio + i) % ALERT_HIST_SIZE;
    AlertEvent &ae = alertHistorial[pos];

    // Convertir millis a H:MM:SS desde el arranque
    uint32_t seg  = ae.ts / 1000;
    uint32_t hh   = seg / 3600;
    uint32_t mm   = (seg % 3600) / 60;
    uint32_t ss   = seg % 60;
    char lbl[12];
    snprintf(lbl, sizeof(lbl), "%02lu:%02lu:%02lu", hh, mm, ss);

    const char* nivelStr = (ae.nivel == AIR_PEL) ? "PEL" : "ADV";
    json += "{\"ts\":\"";
    json += lbl;
    json += "\",\"nivel\":\"";
    json += nivelStr;
    json += "\"}";
    if (i < total - 1) json += ",";
  }
  json += "]";

  server.send(200, "application/json", json);
}

// 404 para rutas no definidas
void handleNotFound() {
  server.send(404, "text/plain", "Ruta no encontrada");
}

// ════════════════════════════════════════════════════════════
//  TAREA FreeRTOS — LECTURA DE SENSORES (Core 0)
//  Lee BME280, MQ135 y PMS5003 cada 2 segundos.
//  Calcula el estado fusionado y guarda en el histórico.
//  Usa mutex para escritura segura de variables globales.
// ════════════════════════════════════════════════════════════
void tareaLecturaSensores(void *pvParameters) {
  const TickType_t periodo = pdMS_TO_TICKS(2000);
  TickType_t       ultimo  = xTaskGetTickCount();

  for (;;) {
    // ── Leer BME280 ────────────────────────────────────────
    float t = bme.readTemperature();
    float h = bme.readHumidity();
    float p = bme.readPressure() / 100.0f;

    // ── Leer MQ135 ─────────────────────────────────────────
    int raw = readMQ135_RAW();

    // ── Leer PMS5003 (todas las tramas disponibles) ─────────
    uint16_t pm = 0;
    PMS5003Data pmsData;
    while (parsePMS5003(pmsData)) {
      pm = pmsData.pm2_5_atm;
    }

    // ── Validar y escribir con mutex ───────────────────────
    if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100))) {

      if (!isnan(t) && t > -40.0f  && t <   85.0f)  g_temperature = t;
      if (!isnan(h) && h >=  0.0f  && h <= 100.0f)  g_humidity    = h;
      if (!isnan(p) && p >= 300.0f && p <= 1013.0f) g_pressure    = p;
      g_rawMQ = raw;
      if (pm > 0) g_pm2_5 = pm;

      g_airState = computeAirState(g_temperature, g_humidity,
                                   g_rawMQ, g_pm2_5);

      // ── Registrar nueva alerta si el estado empeora ────────────
      if (g_airState != AIR_OK && g_airState != g_lastAlertState) {
        alertHistorial[alertHistIdx] = {
          (uint8_t)g_airState,
          (uint32_t)millis()
        };
        alertHistIdx = (alertHistIdx + 1) % ALERT_HIST_SIZE;
        if (alertHistCount < ALERT_HIST_SIZE) alertHistCount++;
      }
      g_lastAlertState = g_airState;

      // ── Guardar en buffer histórico circular ───────────────
      historial[histIdx] = {
        g_temperature, g_humidity, g_pressure,
        g_rawMQ, g_pm2_5, (uint8_t)g_airState,
        (uint32_t)millis()
      };
      histIdx = (histIdx + 1) % HIST_SIZE;
      if (histCount < HIST_SIZE) histCount++;

      xSemaphoreGive(xDataMutex);
    }

    // ── Debug Serial ───────────────────────────────────────
    Serial.printf("T:%.1f H:%.0f P:%.0f RAW:%d PM25:%d EST:%s\n",
                  g_temperature, g_humidity, g_pressure,
                  g_rawMQ, g_pm2_5, STATE_LABEL[g_airState]);

    // Esperar hasta el próximo periodo exacto (no deriva)
    vTaskDelayUntil(&ultimo, periodo);
  }
}

// ════════════════════════════════════════════════════════════
//  TAREA FreeRTOS — SERVIDOR WEB (Core 0)
//  Atiende peticiones HTTP continuamente.
// ════════════════════════════════════════════════════════════
void tareaServidorWeb(void *pvParameters) {
  for (;;) {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(2));  // Cede CPU brevemente
  }
}

// ════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, PMS_RX_PIN, PMS_TX_PIN);

  // ── Pines de salida ────────────────────────────────────────
  pinMode(BUZZER_PIN, OUTPUT);

  // ── LEDC — PWM para LED RGB (API ESP32 Arduino Core v3.x) ──
  // ledcSetup + ledcAttachPin fueron eliminados en Core v3.
  // Usar ledcAttach(pin, freq, resolution) y ledcWrite(pin, duty).
  ledcAttach(LED_R_PIN, PWM_FREQ, PWM_RES);
  ledcAttach(LED_G_PIN, PWM_FREQ, PWM_RES);
  ledcAttach(LED_B_PIN, PWM_FREQ, PWM_RES);

  // ── I2C a 50 kHz ───────────────────────────────────────────
  Wire.begin(21, 22);
  Wire.setClock(50000UL);

  // ── LCD ────────────────────────────────────────────────────
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(" AIR QUALITY MON");
  lcd.setCursor(0, 1);
  lcd.print("  Iniciando...  ");
  setRGB(0, 80, 0);

  // ── BME280 ─────────────────────────────────────────────────
  if (!bme.begin(BME_I2C_ADDR)) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("BME280 no found!");
    lcd.setCursor(0, 1); lcd.print("Check addr/cable");
    setRGB(255, 0, 0);
    while (true) { vTaskDelay(pdMS_TO_TICKS(1000)); }
  }

  bme.setSampling(
    Adafruit_BME280::MODE_NORMAL,
    Adafruit_BME280::SAMPLING_X2,
    Adafruit_BME280::SAMPLING_X16,
    Adafruit_BME280::SAMPLING_X1,
    Adafruit_BME280::FILTER_X16,
    Adafruit_BME280::STANDBY_MS_500
  );

  // ── Conectar WiFi ───────────────────────────────────────────
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Conectando WiFi");
  lcd.setCursor(0, 1); lcd.print(WIFI_SSID);
  setRGB(0, 0, 80);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 30) {
    delay(500);
    intentos++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("WiFi: FALLO");
    lcd.setCursor(0, 1); lcd.print("Solo modo local");
    setRGB(255, 100, 0);
    delay(3000);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("WiFi conectado");
    lcd.setCursor(0, 1); lcd.print(WiFi.localIP().toString());
    Serial.print("IP del dispositivo: ");
    Serial.println(WiFi.localIP());
    setRGB(0, 255, 0);
    delay(3000);
  }

  // ── Rutas del servidor web ──────────────────────────────────
  server.on("/",         HTTP_GET, handleRoot);
  server.on("/data",     HTTP_GET, handleData);
  server.on("/history",  HTTP_GET, handleHistory);
  server.on("/alarm/off",HTTP_GET, handleAlarmOff);
  server.on("/alarm/on", HTTP_GET, handleAlarmOn);
  server.on("/alerts",   HTTP_GET, handleAlerts);
  server.onNotFound(handleNotFound);
  server.begin();

  // ── Mutex de datos ─────────────────────────────────────────
  xDataMutex = xSemaphoreCreateMutex();

  // ── Crear tareas FreeRTOS ───────────────────────────────────
  // Tarea sensores — Core 0, prioridad 2
  xTaskCreatePinnedToCore(
    tareaLecturaSensores,
    "Sensores",
    4096,        // Stack en bytes
    NULL,
    2,           // Prioridad
    NULL,
    0            // Core 0
  );

  // Tarea servidor web — Core 0, prioridad 1
  xTaskCreatePinnedToCore(
    tareaServidorWeb,
    "ServidorWeb",
    8192,        // El servidor necesita más stack
    NULL,
    1,
    NULL,
    0            // Core 0
  );

  lcd.clear();
}

// ════════════════════════════════════════════════════════════
//  LOOP PRINCIPAL (Core 1)
//  Maneja LCD, LED y Buzzer. Las tareas pesadas están
//  en Core 0 gestionadas por FreeRTOS.
// ════════════════════════════════════════════════════════════
void loop() {
  unsigned long now = millis();

  // Leer estado actual con protección de mutex
  AirState estadoActual = AIR_OK;
  if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(10))) {
    estadoActual = g_airState;
    xSemaphoreGive(xDataMutex);
  }

  // ── LED y Buzzer ────────────────────────────────────────────
  updateLED(estadoActual);
  updateBuzzer(estadoActual);

  // ── Cambiar pantalla cada SCREEN_INTERVAL ───────────────────
  if (now - lastScreenSwitch >= SCREEN_INTERVAL) {
    lastScreenSwitch = now;
    g_currentScreen  = 1 - g_currentScreen;
    lcd.clear();
    lastLCDUpdate = 0;
  }

  // ── Refrescar LCD cada LCD_UPDATE_MS ───────────────────────
  if (now - lastLCDUpdate >= LCD_UPDATE_MS) {
    lastLCDUpdate = now;
    updateLCD();
  }

  delay(5);
}
