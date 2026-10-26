/*
 * ESP32-S3 心知天气看板
 * ------------------------------------------------------------
 * 对应教程：JSON 解析 → HTTPS 请求 → ArduinoJson 取值 → WebServer 建站
 *
 * 做的事：
 *   1. 连 WiFi
 *   2. 每 10 分钟用 HTTPS 拉一次心知天气「实况 + 3 天预报」
 *   3. 用 ArduinoJson 解析，串口打印（教程第六章）
 *   4. 板子自己开一个网页，同一 WiFi 下手机可访问（教程第七章）
 *   5. 额外提供 /api/weather，返回 JSON 给前端调用
 *
 * 依赖库（库管理器搜索安装）：
 *   - ArduinoJson  (Benoît Blanchon)  7.x
 *
 * 板型：ESP32-S3（ESP32 全系通用）
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>

#include "config.h"          // 复制 config.h.example 改名后填自己的信息

WebServer server(80);

// ── 解析后的天气数据（全局，串口与网页共用同一份）──
struct Weather {
  String city   = "—";
  String text   = "—";
  String temp   = "—";
  String code   = "—";
  String update = "—";
  String day1Text = "—", day1High = "—", day1Low = "—";
  String day2Text = "—", day2High = "—", day2Low = "—";
  String day3Text = "—", day3High = "—", day3Low = "—";
  bool   valid  = false;
} wx;

String rawNowJson = "{}";    // 原始 JSON，网页「查看原文」用
unsigned long lastFetch = 0;
const unsigned long FETCH_INTERVAL = 10UL * 60UL * 1000UL;   // 10 分钟


/* ────────────────────────── 网络请求 ────────────────────────── */

// 通用 HTTPS GET：返回响应体，失败返回空串
String httpsGet(const String &url) {
  WiFiClientSecure client;
  client.setInsecure();               // 跳过证书校验，省证书体积；生产环境建议装根证书
  HTTPClient http;

  if (!http.begin(client, url)) {
    Serial.println(">> begin() 失败");
    return "";
  }
  http.setTimeout(12000);
  int code = http.GET();
  Serial.printf(">> HTTP 状态码：%d\n", code);

  String body;
  if (code == 200) body = http.getString();
  http.end();
  return body;
}


/* ────────────────────────── 抓取并解析 ────────────────────────── */

void fetchWeather() {
  Serial.println("\n======== 开始请求天气 ========");

  String base = "https://api.seniverse.com/v3/weather/";
  String q    = "?key=" + String(SENIVERSE_KEY) +
                "&location=" + String(CITY) +
                "&language=zh-Hans&unit=c";

  // ① 实况
  String payload = httpsGet(base + "now.json" + q);
  if (payload.isEmpty()) { Serial.println(">> 实况请求失败"); return; }
  rawNowJson = payload;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {                                   // 教程强调：先判错再取值
    Serial.printf(">> JSON 解析失败：%s\n", err.c_str());
    return;
  }

  JsonObject r0 = doc["results"][0];           // results 是数组，取第 0 个
  wx.city   = r0["location"]["name"]     | "—";
  wx.text   = r0["now"]["text"]          | "—";
  wx.temp   = r0["now"]["temperature"]   | "—";   // 注意：接口返回的是字符串
  wx.code   = r0["now"]["code"]          | "—";
  wx.update = r0["last_update"].as<String>();
  wx.valid  = true;

  // ② 三天预报（失败不影响实况）
  String dailyPayload = httpsGet(base + "daily.json" + q + "&start=0&days=3");
  if (!dailyPayload.isEmpty()) {
    JsonDocument d2;
    if (!deserializeJson(d2, dailyPayload)) {
      JsonArray days = d2["results"][0]["daily"];
      if (days.size() >= 3) {
        wx.day1Text = days[0]["text_day"] | "—";
        wx.day1High = days[0]["high"]     | "—";
        wx.day1Low  = days[0]["low"]      | "—";
        wx.day2Text = days[1]["text_day"] | "—";
        wx.day2High = days[1]["high"]     | "—";
        wx.day2Low  = days[1]["low"]      | "—";
        wx.day3Text = days[2]["text_day"] | "—";
        wx.day3High = days[2]["high"]     | "—";
        wx.day3Low  = days[2]["low"]      | "—";
      }
    }
  }

  // 串口输出（教程第六章的效果）
  Serial.println("======== 心知天气 ========");
  Serial.printf("城市：%s\n", wx.city.c_str());
  Serial.printf("天气：%s    温度：%s ℃   代码：%s\n",
                wx.text.c_str(), wx.temp.c_str(), wx.code.c_str());
  Serial.printf("更新时间：%s\n", wx.update.c_str());
  Serial.printf("明天：%s  %s~%s ℃\n", wx.day2Text.c_str(), wx.day2Low.c_str(), wx.day2High.c_str());
  Serial.println("==========================");

  lastFetch = millis();
}


/* ────────────────────────── 网页 ────────────────────────── */

String buildPage() {
  String h;
  h.reserve(2600);
  h += F("<!DOCTYPE html><html lang=zh-CN><head><meta charset=utf-8>");
  h += F("<meta name=viewport content='width=device-width,initial-scale=1,viewport-fit=cover'>");
  h += F("<meta name=theme-color content='#0b1020'>");
  h += F("<title>");
  h += wx.city;
  h += F(" 实时天气</title><style>");
  h += F("*{margin:0;padding:0;box-sizing:border-box}");
  h += F("body{min-height:100vh;font-family:-apple-system,'PingFang SC',system-ui,sans-serif;");
  h += F("color:#fff;display:flex;align-items:center;justify-content:center;padding:22px;");
  h += F("background:radial-gradient(120% 60% at 15% -10%,#1e3aa0,transparent 55%),");
  h += F("radial-gradient(95% 50% at 95% 5%,#84123e,transparent 58%),");
  h += F("radial-gradient(120% 60% at 45% 110%,#9e5414,transparent 60%),#070a12}");
  h += F(".wrap{width:100%;max-width:400px}");
  h += F(".card{border-radius:26px;padding:20px;border:1px solid rgba(255,255,255,.85);");
  h += F("background:rgba(255,255,255,.85);color:#14161c;");
  h += F("-webkit-backdrop-filter:blur(50px) saturate(190%);backdrop-filter:blur(50px) saturate(190%);");
  h += F("box-shadow:0 26px 60px -22px rgba(6,10,30,.6),inset 0 1px 0 #fff}");
  h += F(".city{font-size:12px;letter-spacing:1px;color:rgba(24,28,40,.5);text-transform:uppercase}");
  h += F(".temp{font-size:64px;font-weight:300;letter-spacing:-2px;line-height:1.05;margin-top:6px}");
  h += F(".temp i{font-size:20px;font-style:normal;color:rgba(24,28,40,.6)}");
  h += F(".cond{font-size:15px;color:rgba(24,28,40,.7);margin-top:4px}");
  h += F(".up{font-size:11.5px;color:rgba(24,28,40,.42);margin-top:12px}");
  h += F(".fc{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-top:16px}");
  h += F(".d{border-radius:15px;padding:12px 8px;text-align:center;");
  h += F("background:rgba(255,255,255,.62);border:1px solid rgba(255,255,255,.85)}");
  h += F(".d b{display:block;font-size:14px}.d span{font-size:12px;color:rgba(24,28,40,.45)}");
  h += F(".d p{font-size:11.5px;color:rgba(24,28,40,.6);margin:5px 0 6px}");
  h += F("</style></head><body><div class=wrap><div class=card>");
  h += F("<div class=city>"); h += wx.city; h += F("</div>");
  h += F("<div class=temp>"); h += wx.temp; h += F("<i>℃</i></div>");
  h += F("<div class=cond>"); h += wx.text; h += F("　·　代码 "); h += wx.code; h += F("</div>");
  h += F("<div class=up>"); h += wx.update; h += F(" 更新 · 每 10 分钟自动刷新</div>");
  h += F("<div class=fc>");
  h += F("<div class=d><b>今天</b><p>"); h += wx.day1Text; h += F("</p><span>");
  h += wx.day1Low; h += F("° / "); h += wx.day1High; h += F("°</span></div>");
  h += F("<div class=d><b>明天</b><p>"); h += wx.day2Text; h += F("</p><span>");
  h += wx.day2Low; h += F("° / "); h += wx.day2High; h += F("°</span></div>");
  h += F("<div class=d><b>后天</b><p>"); h += wx.day3Text; h += F("</p><span>");
  h += wx.day3Low; h += F("° / "); h += wx.day3High; h += F("°</span></div>");
  h += F("</div></div></div></body></html>");
  return h;
}

void handleRoot() {
  if (!wx.valid) {
    server.send(200, "text/html",
      "<meta charset=utf-8><body style='font-family:system-ui;padding:30px'>"
      "<h3>正在获取天气…</h3><meta http-equiv=refresh content=5>");
    return;
  }
  String page = buildPage();
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/html; charset=utf-8", page);
}

void handleApi() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-store");
  if (!wx.valid) { server.send(503, "application/json", "{\"status\":\"not ready\"}"); return; }
  server.send(200, "application/json; charset=utf-8", rawNowJson);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}


/* ────────────────────────── 生命周期 ────────────────────────── */

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n== 系统启动，波特率 115200 ==");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("WiFi 连接中 ");
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries++ < 40) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi 已连接，IP：%s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("WiFi 连接失败，检查 SSID / 密码");
  }

  fetchWeather();

  server.on("/", handleRoot);
  server.on("/api/weather", handleApi);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.printf("网页服务已启动：http://%s/\n", WiFi.localIP().toString().c_str());
}

void loop() {
  server.handleClient();                       // 处理手机发来的请求

  if (millis() - lastFetch > FETCH_INTERVAL) { // 定时刷新（教程建议 10 分钟）
    if (WiFi.status() == WL_CONNECTED) fetchWeather();
    else lastFetch = millis();
  }
}
