# 实时天气 · ESP32

把「用 JSON 让开发板读懂互联网数据」这个教程做成了一个能装到手机上的天气看板。
界面与 [esp32-glass](https://github.com/onepro07/esp32-glass) 同一套玻璃拟态设计语言。

**线上地址：https://onepro07.github.io/esp32-weather/**

---

## 它对应教程的哪些内容

| 教程章节 | 在这个项目里体现为 |
| --- | --- |
| 二、JSON 怎么写 | 页面底部「接口返回的 JSON 原文」可展开，带语法高亮 |
| 三、下标 [0] 和 [1] | 底部标注了取值路径 `results[0].now.temperature` |
| 五、ArduinoJson 解析 | `firmware/` 里的固件，串口打印同一份数据 |
| 六、心知天气实战 | `firmware/` 按教程接入心知天气；网页改用 Open-Meteo（免密钥，见下） |
| 七、开发板当网站 | 固件用 `WebServer` 把天气渲染成网页 |

---

## 网页怎么用

打开线上地址**就是实时天气**，不用注册、不用填密钥。

**搜城市**：顶部搜索框输入任意地名——中文（石家庄）、拼音（shijiazhuang）、
英文（Tianjin）都行，回车或点右侧按钮查询。搜过的城市会以「最近」的形式留在下方，
方便一键切回（最多 5 个）。

数据缓存 10 分钟，点右上角刷新可强制重取。

### 数据从哪来

| 数据 | 来源 | 说明 |
| --- | --- | --- |
| 实况、三天预报 | [Open-Meteo](https://open-meteo.com/) | 免密钥、免注册，`access-control-allow-origin: *`，浏览器可直连 |
| 城市名 → 经纬度 | Open-Meteo Geocoding | 支持中文、拼音、英文 |
| 生活指数六项 | **本地推算** | 按温度、紫外线、降水概率计算，不是官方数据 |

这也解释了为什么网页不需要私钥：Open-Meteo 是开放接口，而心知天气的接口必须带 key。
固件仍按教程走心知天气，两条路互不影响。

### 装到手机桌面

- **iPhone / Safari**：分享 → 添加到主屏幕
- **安卓 / Chrome**：菜单 → 安装应用

装好后全屏启动，无地址栏；首次访问后支持离线打开（天气数据本身仍需联网）。

---

## 目录

```
esp32-weather/
├── index.html              # 单文件网页（无外部依赖，26 KB）
├── manifest.webmanifest    # PWA 清单
├── sw.js                   # 离线缓存（天气接口不缓存）
├── icons/                  # 应用图标
├── make_icons.py           # 图标生成脚本
└── firmware/
    └── esp32_weather/
        ├── esp32_weather.ino
        ├── config.h.example
        └── .gitignore
```

---

## 固件

两种玩法，按需要选：

**方案 A：板子只当静态服务器（最省事）**
把 `index.html`、`manifest.webmanifest`、`sw.js`、`icons/` 一起上传到开发板 LittleFS，
手机连开发板热点后访问 `192.168.4.1`。网页自己调 Open-Meteo，**不需要密钥**，
但开发板必须能联网（STA 模式连路由器，而不是只开热点）。

**方案 B：板子自己拉数据（教程原版）**
烧 `firmware/esp32_weather/esp32_weather.ino`，板子每 10 分钟自己请求一次，
串口打印解析结果，并开一个网页给手机看。

```bash
cd firmware/esp32_weather
cp config.h.example config.h    # 然后填入 WiFi、私钥、城市
```

用 Arduino IDE 烧录，需要装库 **ArduinoJson 7.x**。烧完打开串口监视器（115200），
会看到和教程一模一样的输出：

```
======== 心知天气 ========
城市：北京
天气：晴    温度：24 ℃   代码：0
更新时间：2026-10-26T10:00:00+08:00
```

> `config.h` 已写进 `.gitignore`，填了密钥也不会被提交。

### 相比教程代码的两处修正

1. **HTTPS 要带 `WiFiClientSecure`**。教程里直接 `http.begin(url)` 对 `https://` 地址
   在 ESP32 上会连不上——必须传 client：
   ```cpp
   WiFiClientSecure client;
   client.setInsecure();          // 跳过证书校验
   http.begin(client, url);
   ```
   生产环境建议换成加载根证书，`setInsecure()` 只为省 Flash。
2. **`temperature` 是字符串不是数字**。接口返回 `"24"` 而不是 `24`，
   直接塞进 `float` 会得到 0，得用 `.as<String>()` 或 `atoi()`。

---

## JSON 速记（教程前四章）

```json
{
  "sites": [
    { "name": "json",   "url": "www.json.cn" },
    { "name": "google", "url": "www.google.com" }
  ]
}
```

- 数据在**名称/值对**中，名称必须双引号
- 多项之间用**逗号**，**最后一项不能有逗号**（最常见的报错）
- `{ }` 是对象，`[ ]` 是数组
- 值的类型只有六种：字符串、数值、布尔、null、对象、数组
- 下标**从 0 开始**，`sites[0].name` 拿到 `"json"`

页面里的取值路径（网页把 Open-Meteo 响应整理成了同一套结构，方便对照教程）：

```
results[0].location.name        → 城市
results[0].now.text             → 天气现象
results[0].now.temperature      → 温度
results[0].last_update          → 更新时间
```

---

## 说明

- Open-Meteo 返回的 CORS 头是 `access-control-allow-origin: *`，浏览器可以直接调，无需自建代理
- 网页显示的湿度、体感温度来自接口；生活指数是本地推算，界面右上角标了「按温湿度推算」
- 心知天气免费版只覆盖国内 370 个主要城市，Open-Meteo 覆盖全球，小地方也能查到
