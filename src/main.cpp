// #include "esp_camera.h"
// #include <WiFi.h>
// #include "Drv/ESP32_OV5640_AF.h"
// #include <WebServer.h>
// #include <Arduino.h>
// #include <ESP32Servo.h>


// #define SERVO_PIN 13 // 连接舵机的引脚
// #define LED_GPIO 4 // LED灯的GPIO引脚（根据你的开发板确认）


// // 创建 Servo 对象
// Servo servo;

// int currentAngle = 0;
// int targetAngle = 90;
// int servoState_1 = 0; // 0 表示空闲, 1 表示旋转到目标位置, 2 表示返回初始位
// unsigned long lastUpdateTime = 0;
// unsigned long updateInterval = 1000; // 每次旋转更新的时间间隔，单位：毫秒


// void rotateServo() 
// {
//   if (millis() - lastUpdateTime >= updateInterval) {
//     lastUpdateTime = millis();
//     Serial.printf("%d %d", servoState_1, currentAngle);
//     switch (servoState_1) {
//       case 0: // 空闲状态
//         // 开始旋转到目标位置
//         servoState_1 = 1;
//         break;
//       case 1: // 旋转到目标位置
//         if (currentAngle < targetAngle) {
//           currentAngle = currentAngle + 5;
//           servo.write(currentAngle);
//         } else {
//           // 到达目标位置后切换状态
//           servoState_1 = 2;
//           lastUpdateTime = millis(); // 重置时间
//         }
//         break;
//       case 2: // 等待1秒然后返回初始位置
//         if (currentAngle > 0) {
//           currentAngle = currentAngle - 5;
//           servo.write(currentAngle);
//         } else {
//           // 返回初始位置后切换状态
//           servoState_1 = 0;
//           lastUpdateTime = millis(); // 重置时间
//         }
//         break;
//       default:
//         break;
//     }
//   }
// }

// void setup() 
// {
//   Serial.begin(115200);

//   // 设置舵机连接的引脚
//   servo.attach(SERVO_PIN);
//   servo.write(0); // 舵机归位

//   // 初始化LED引脚为输出模式
//   pinMode(LED_GPIO, OUTPUT);
//     // 打开LED灯
//   digitalWrite(LED_GPIO, HIGH);
// }

// void loop() 
// {
//   rotateServo();
// }

#include "esp_camera.h"
#include <WiFi.h>
#include "Drv/ESP32_OV5640_AF.h"
#include <WebServer.h>
#include <Arduino.h>
#include <ESP32Servo.h>

#define SERVO_PIN 13 // 连接舵机的引脚

// LED灯的GPIO引脚（根据你的开发板确认）
#define LED_GPIO 4

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

OV5640 ov5640 = OV5640();
WebServer server(80);

const char* ssid = "ESP32_Camera";
const char* password = "12345678";

// 函数声明
void handle_jpg_stream();
void handle_capture();

void setup() {
  Serial.begin(115200);

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
        <title>ESP32-CAM</title>
        <script>
          function capturePhoto() {
            fetch('/capture').then(response => response.blob()).then(blob => {
              document.getElementById('photo').src = URL.createObjectURL(blob);
            });
          }
        </script>
      </head>
      <body>
        <h1>ESP32-CAM</h1>
        <button onclick="capturePhoto()">Capture Photo</button>
        <br><br>
        <img id="photo" src="" style="width: 640px; height: 480px;">
      </body>
      </html>
    )rawliteral");
  });
  server.on("/stream", HTTP_GET, handle_jpg_stream);
  server.on("/capture", HTTP_GET, handle_capture);
  server.begin();
  Serial.println("HTTP server started");
  Serial.print("Camera Stream Ready! Go to: http://");
  Serial.print(IP);
  Serial.println("/");
}

void loop() {
  server.handleClient();
}

void handle_jpg_stream() {
  WiFiClient client = server.client();
  camera_fb_t * fb = NULL;
  static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=frame";
  static const char* _STREAM_BOUNDARY = "\r\n--frame\r\n";
  static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, _STREAM_CONTENT_TYPE);

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }

    if (fb->format != PIXFORMAT_JPEG) {
      Serial.println("Non-JPEG data not implemented");
      esp_camera_fb_return(fb);
      return;
    }
    size_t fb_len = fb->len;
    server.sendContent(_STREAM_BOUNDARY);
    char part_buf[64];
    snprintf(part_buf, 64, _STREAM_PART, fb_len);
    server.sendContent(part_buf);
    server.sendContent((const char *)fb->buf, fb_len);
    esp_camera_fb_return(fb);

    if (!client.connected()) {
      break;
    }
  }
}

void handle_capture() {
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

  // 发送图片头信息
  server.sendHeader("Content-Disposition", "inline; filename=capture.jpg");
  server.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);

  esp_camera_fb_return(fb);
}

