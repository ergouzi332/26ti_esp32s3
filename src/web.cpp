#include "web.h"
#include "ball.h"
#include "k230.h"
#include "stepper.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <stdarg.h>
#include <string.h>

#define WEB_ENABLE 1
#define WEB_SSID   "26TI-DBG"
#define WEB_PASS   "12345678"
#define LOG_MAX    160
#define LOG_LEN    100

extern volatile uint8_t  g_ballX;
extern volatile uint16_t g_runMs;
extern volatile uint8_t  g_timerRun;
extern volatile uint8_t  g_stop;
extern volatile uint8_t  g_ballCmd;
extern volatile uint8_t  g_webCmd;
extern volatile uint8_t  g_lastCmd;

static char          s_log[LOG_MAX][LOG_LEN];
static volatile uint16_t s_head = 0;
static volatile uint16_t s_cnt  = 0;
static portMUX_TYPE   s_mux = portMUX_INITIALIZER_UNLOCKED;

static WebServer server(80);

void Web_Logf(const char *fmt, ...) {
    char tmp[LOG_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, LOG_LEN, fmt, ap);
    va_end(ap);
    int len = (int)strlen(tmp);
    while (len > 0 && (tmp[len - 1] == '\n' || tmp[len - 1] == '\r')) tmp[--len] = 0;
    Serial.println(tmp);
    portENTER_CRITICAL(&s_mux);
    uint16_t i = s_head;
    strncpy(s_log[i], tmp, LOG_LEN - 1);
    s_log[i][LOG_LEN - 1] = 0;
    s_head = (uint16_t)((i + 1) % LOG_MAX);
    if (s_cnt < LOG_MAX) s_cnt++;
    portEXIT_CRITICAL(&s_mux);
}

static void sendApi() {
    int16_t x = (int16_t)g_ballX;
    bool lost = (x == K230_LOST);
    String j = "{\"ball\":";
    if (lost) j += "null"; else j += String(x);
    j += ",\"cm\":";
    if (lost) j += "null"; else j += String(X_CM(x), 2);
    j += ",\"phase\":";
    j += String((int)Ball_GetPhase());
    j += ",\"tcm\":";
    j += String(X_CM(Ball_GetTargetX()), 2);
    j += ",\"steps\":";
    j += String((int32_t)Stepper_GetSteps());
    j += ",\"tSteps\":";
    j += String((int32_t)Stepper_GetTarget());
    j += ",\"at\":";
    j += String(Stepper_AtTarget() ? 1 : 0);
    j += ",\"ms\":";
    j += String((unsigned)g_runMs);
    j += ",\"timer\":";
    j += String(g_timerRun);
    j += ",\"stop\":";
    j += String(g_stop);
    j += ",\"lastCmd\":";
    j += String((unsigned)g_lastCmd);
    j += ",\"up\":";
    j += String((uint32_t)(millis() / 1000));
    j += "}";
    server.send(200, "application/json", j);
}

static void sendLog() {
    uint16_t from = 0;
    if (server.hasArg("from")) from = (uint16_t)server.arg("from").toInt();
    uint16_t head, cnt;
    portENTER_CRITICAL(&s_mux);
    head = s_head;
    cnt  = (s_cnt < LOG_MAX) ? s_cnt : LOG_MAX;
    portEXIT_CRITICAL(&s_mux);
    uint16_t oldest = (cnt < LOG_MAX) ? 0 : head;
    if (from < oldest) from = oldest;
    String j = "{\"n\":";
    j += String((unsigned)head);
    j += ",\"l\":[";
    uint16_t k = 0;
    char tmp[LOG_LEN];
    for (uint16_t i = from; i != head && k < 200; i = (uint16_t)((i + 1) % LOG_MAX)) {
        portENTER_CRITICAL(&s_mux);
        memcpy(tmp, s_log[i], LOG_LEN);
        portEXIT_CRITICAL(&s_mux);
        tmp[LOG_LEN - 1] = 0;
        if (k) j += ",";
        j += "\"";
        for (char *c = tmp; *c; c++) {
            if (*c == '"' || *c == '\\') j += '\\';
            j += *c;
        }
        j += "\"";
        k++;
    }
    j += "]}";
    server.send(200, "application/json", j);
}

static const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>26TI Log</title>
<style>
body{background:#0d1117;color:#e6edf3;font-family:Consolas,monospace;margin:0;padding:10px;font-size:13px}
#st{display:inline-block;width:10px;height:10px;border-radius:50%;background:#f00;margin-right:8px}
#status{white-space:pre;margin:6px 0;color:#f0b429}
#log{background:#010409;border:1px solid #21262d;border-radius:8px;padding:8px;height:52vh;overflow-y:auto;white-space:pre-wrap;word-break:break-all;line-height:1.45}
#log div{color:#7ee787}
button{margin:4px 4px 0 0;padding:9px 12px;font-size:13px;border:0;border-radius:6px;background:#238636;color:#fff;cursor:pointer}
button.red{background:#da3633}
</style>
</head>
<body>
<div><span id="st"></span><b>26TI Serial Log</b> <span id="up"></span></div>
<div id="status">-</div>
<div id="log"></div>
<div>
<button onclick="cmd(1)">KEY1 LINE</button>
<button onclick="cmd(4)">KEY2 Q3</button>
<button onclick="cmd(5)">KEY3 Q4</button>
<button class="red" onclick="cmd(6)">Q4 STOP</button>
<button class="red" onclick="cmd(3)">RESET(??)</button>
<button class="red" onclick="cmd(2)">LINE STOP</button>
<button class="red" onclick="clr()">CLEAR LOG</button>
</div>
<script>
var cur=0;
var clearing=false;
function pollLog(){
 if(clearing)return;
 fetch('/log?from='+cur).then(function(r){return r.json()}).then(function(d){
  if(d.n!==cur){
   cur=d.n;
   var box=document.getElementById('log');
   var stick=box.scrollTop+box.clientHeight>=box.scrollHeight-30;
   for(var i=0;i<d.l.length;i++){
    var div=document.createElement('div');
    div.textContent=d.l[i];
    box.appendChild(div);
   }
   while(box.childNodes.length>300)box.removeChild(box.firstChild);
   if(stick)box.scrollTop=box.scrollHeight;
  }
 }).catch(function(){});
}
function pollApi(){
 fetch('/api').then(function(r){return r.json()}).then(function(d){
  document.getElementById('st').style.background='#0f0';
  var m=Math.floor(d.up/60),s=d.up%60;
  document.getElementById('up').textContent=m+':'+(s<10?'0':'')+s;
  var ph=['idle','Q3:+5cm','Q3:-5cm','Q3 done','Q4 hold'];
  document.getElementById('status').textContent=
   'cm:'+(d.cm===null?'LOST':d.cm)+'  ph:'+(ph[d.phase]||d.phase)+
   '  T:'+d.tcm+'  steps:'+d.steps+'/'+d.tSteps+(d.at?' OK':'...')+
   '  t:'+d.ms+'ms'+(d.timer?' RUN':'')+
   '  cmd:0x'+(d.lastCmd.toString(16).toUpperCase());
 }).catch(function(){document.getElementById('st').style.background='#f00';});
}
function clr(){
 clearing=true;
 document.getElementById('log').innerHTML='';
 fetch('/log?from=0').then(function(r){return r.json()}).then(function(d){cur=d.n;clearing=false;}).catch(function(){clearing=false;});
}
function cmd(c){fetch('/cmd?c='+c);}
setInterval(pollLog,250);
setInterval(pollApi,200);
pollLog();pollApi();
</script>
</body>
</html>
)rawliteral";

static void Web_Task(void *pv) {
    for (;;) {
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void Web_Init() {
#if WEB_ENABLE
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WEB_SSID, WEB_PASS);
    IPAddress ip = WiFi.softAPIP();
    server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", PAGE); });
    server.on("/api", HTTP_GET, sendApi);
    server.on("/log", HTTP_GET, sendLog);
    server.on("/cmd", HTTP_GET, []() {
        if (server.hasArg("c")) {
            int c = server.arg("c").toInt();
            if (c >= 1 && c <= 9) g_webCmd = (uint8_t)c;
        }
        server.send(200, "text/plain", "ok");
    });
    server.begin();
    xTaskCreatePinnedToCore(Web_Task, "web", 8192, NULL, 1, NULL, 1);
    Web_Logf("[WEB] AP %s pwd %s ip %d.%d.%d.%d",
             WEB_SSID, WEB_PASS, ip[0], ip[1], ip[2], ip[3]);
#else
    Serial.println("[WEB] disabled");
#endif
}
