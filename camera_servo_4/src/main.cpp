#include "esp_camera.h"
#include <WiFi.h>
#include "Drv/ESP32_OV5640_AF.h"
#include <WebServer.h>
#include <Arduino.h>
#include <ESP32Servo.h>

#define SERVO_PIN 13 // 连接舵机的引脚
#define LED_GPIO 4   // LED灯的GPIO引脚（根据你的开发板确认）

#define PWDN_GPIO_NUM    32
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27
#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      21
#define Y4_GPIO_NUM      19
#define Y3_GPIO_NUM      18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22

// 创建 Servo 对象
Servo servo;

int currentAngle = 0;
int targetAngle = 0;
int servoState_1 = 0; // 0 表示空闲, 1 表示旋转到目标位置, 2 表示返回初始位
unsigned long lastUpdateTime = 0;
unsigned long updateInterval = 20; // 每次旋转更新的时间间隔，单位：毫秒
unsigned long photoInterval = 1000; // 默认每 1 秒拍摄一次照片
unsigned int frameRate = 1; // 默认每秒 1 张照片
bool capturing = false; // 是否正在拍摄
int photoCount = 0; // 拍摄的照片计数

OV5640 ov5640 = OV5640();
WebServer server(80);

const char* ssid = "ESP32_Camera";
const char* password = "12345678";

void rotateServo() 
{
  if (millis() - lastUpdateTime >= updateInterval) 
  {
    lastUpdateTime = millis();
    Serial.printf("%d %d\n", servoState_1, currentAngle);
    switch (servoState_1) {
      case 0: // 空闲状态
        break;
      case 1: // 旋转到目标位置
        if (currentAngle < targetAngle) {
          currentAngle += 1;
          servo.write(currentAngle);
        } else if (currentAngle > targetAngle) {
          currentAngle -= 1;
          servo.write(currentAngle);
        } else {
          servoState_1 = 0;
          lastUpdateTime = millis();
        }
        break;
      case 2: // 返回初始位置
        if (currentAngle > 0) {
          currentAngle -= 1;
          servo.write(currentAngle);
        } else {
          servoState_1 = 0;
          lastUpdateTime = millis();
        }
        break;
      default:
        break;
    }
  }
}

void handle_capture() {
  if (!capturing) {
    server.send(400, "text/plain", "拍摄功能已关闭");
    return;
  }
  
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    server.send(500, "text/plain", "Camera capture failed");
    return;
  }

  if (fb->format != PIXFORMAT_JPEG) {
    Serial.println("Non-JPEG data not implemented");
    esp_camera_fb_return(fb);
    server.send(500, "text/plain", "Non-JPEG data not implemented");
    return;
  }

  String imgName = String(millis()) + ".jpg"; // Use timestamp as image name
  server.sendHeader("Content-Disposition", "inline; filename=" + imgName);
  server.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);

  esp_camera_fb_return(fb);
}

void handle_set_angle() {
  if (server.hasArg("angle")) {
    targetAngle = server.arg("angle").toInt();
    servoState_1 = 1; // 设置为旋转到目标位置
    server.send(200, "text/plain", "正在旋转到 " + String(targetAngle) + " 度");
  } else {
    server.send(400, "text/plain", "缺少角度参数");
  }
}

void handle_set_frame_rate() {
  if (server.hasArg("frame_rate")) {
    frameRate = server.arg("frame_rate").toInt();
    photoInterval = 1000 / frameRate; // 更新拍摄间隔
    server.send(200, "text/plain", "帧率设置为 " + String(frameRate) + " 张每秒");
  } else {
    server.send(400, "text/plain", "缺少帧率参数");
  }
}

void handle_start_capturing() {
  capturing = true;
  photoCount = 0; // 重置照片计数
  server.send(200, "text/plain", "拍摄功能已开启");
}

void handle_stop_capturing() {
  capturing = false;
  // 清除照片和编号
  server.send(200, "text/plain", "拍摄功能已关闭");
}

void setup() 
{
  Serial.begin(115200);

  // 设置舵机连接的引脚
  servo.attach(SERVO_PIN);
  servo.write(0); // 舵机归位

  // Camera配置
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA; 
  config.jpeg_quality = 12;
  config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t* sensor = esp_camera_sensor_get();
  ov5640.start(sensor);

  // 配置ESP32为AP模式
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // 设置Web服务器路由
  server.on("/", HTTP_GET, [](){
    server.send_P(200, "text/html", R"rawliteral(
      <html>
      <head>
        <meta charset="UTF-8">
        <title>ESP32-CAM</title>
        <style>
          #photos {
            width: 800px;
            height: 600px;
            overflow: hidden;
            display: flex;
            flex-wrap: wrap;
            box-sizing: border-box;
          }
          #photos div {
            box-sizing: border-box;
            flex: 1 0 20%; /* 每行显示5张图片 */
            margin: 5px;
            position: relative;
          }
          #photos img {
            width: 100%;
            height: auto;
            object-fit: cover;
          }
          #photos div div {
            position: absolute;
            bottom: 5px;
            left: 5px;
            color: white;
            background-color: black;
            padding: 2px 5px;
            border-radius: 3px;
          }
        </style>
        <script>
          let photoCount = 0;
          let capturing = false;
          let frameRate = 1;
          let captureInterval;

          function capturePhoto() {
            if (!capturing) return;
            fetch('/capture').then(response => response.blob()).then(blob => {
              let img = document.createElement('img');
              img.src = URL.createObjectURL(blob);
              let photosDiv = document.getElementById('photos');
              if (photosDiv.childElementCount >= 10) {
                photosDiv.removeChild(photosDiv.firstChild);
              }
              let photoWrapper = document.createElement('div');
              let number = document.createElement('div');
              number.innerText = photoCount + 1;
              photoWrapper.appendChild(img);
              photoWrapper.appendChild(number);
              photosDiv.appendChild(photoWrapper);
              photoCount++;
            });
          }

          function startCapturing() {
            fetch('/start_capturing').then(response => response.text()).then(text => {
              alert(text);
              capturing = true;
              photoCount = 0; // 重置照片计数
              clearInterval(captureInterval);
              captureInterval = setInterval(capturePhoto, 1000 / frameRate);
            });
          }

          function stopCapturing() {
            fetch('/stop_capturing').then(response => response.text()).then(text => {
              alert(text);
              capturing = false;
              clearInterval(captureInterval);
              let photosDiv = document.getElementById('photos');
              while (photosDiv.firstChild) {
                photosDiv.removeChild(photosDiv.firstChild);
              }
              photoCount = 0; // 清除计数器
            });
          }

          function setAngle() {
            let angle = document.getElementById('angle').value;
            fetch('/set_angle?angle=' + angle).then(response => response.text()).then(text => {
              alert(text);
            });
          }

          function returnTo0() {
            fetch('/return_to_0').then(response => response.text()).then(text => {
              alert(text);
            });
          }

          function setFrameRate() {
            let newFrameRate = document.getElementById('frame_rate').value;
            window.frameRate = newFrameRate;
            if (capturing) {
              clearInterval(captureInterval);
              captureInterval = setInterval(capturePhoto, 1000 / window.frameRate);
            }
            fetch('/set_frame_rate?frame_rate=' + newFrameRate).then(response => response.text()).then(text => {
              alert(text);
            });
          }
        </script>
      </head>
      <body>
        <h1>ESP32-CAM</h1>

        <input type="number" id="angle" placeholder="输入角度 (0-180)" min="0" max="180">
        <input type="number" id="frame_rate" placeholder="输入帧率 (每秒张数)" min="1">
        <button onclick="setAngle()">发送</button>
        <button onclick="returnTo0()">返回初始化</button>
        <button onclick="setFrameRate()">设置帧率</button>
        <button onclick="startCapturing()">开启拍摄</button>
        <button onclick="stopCapturing()">关闭拍摄</button>
        <div id="photos"></div>
        <br><br>
      </body>
      </html>
    )rawliteral");
  });

  server.on("/capture", HTTP_GET, handle_capture);
  server.on("/set_angle", HTTP_GET, handle_set_angle);
  server.on("/set_frame_rate", HTTP_GET, handle_set_frame_rate);
  server.on("/return_to_0", HTTP_GET, [](){
    targetAngle = 0;
    servoState_1 = 1; // 设置为返回到初始位置
    server.send(200, "text/plain", "正在返回到 0 度");
  });
  server.on("/start_capturing", HTTP_GET, handle_start_capturing);
  server.on("/stop_capturing", HTTP_GET, handle_stop_capturing);

  server.begin();
  Serial.println("HTTP server started");
  Serial.print("Camera Stream Ready! Go to: http://");
  Serial.print(WiFi.softAPIP());
  Serial.println("/");
}

void loop() 
{
  rotateServo();
  server.handleClient();
}
