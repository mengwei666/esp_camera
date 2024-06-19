#include "esp_camera.h"
#include <WiFi.h>
#include "Drv/ESP32_OV5640_AF.h"
#include <WebServer.h>
#include <Arduino.h>
#include <ESP32Servo.h>

#define SERVO_PIN 13 // 连接舵机的引脚

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

// 定义舵机状态
enum ServoState {
  SERVO_IDLE,
  SERVO_ROTATING
};

ServoState servoState = SERVO_IDLE;
unsigned long lastRotateTime = 0;
const unsigned long rotateInterval = 5000; // 舵机旋转间隔时间，单位：毫秒

OV5640 ov5640 = OV5640();
WebServer server(80);

const char* ssid = "ESP32_Camera";
const char* password = "12345678";

unsigned long lastPrintTime = 0;
const unsigned long printInterval = 2000; // 打印间隔时间，单位：毫秒

// 函数声明
void rotateServo();
void handle_jpg_stream();

void setup() {
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
  server.on("/stream", HTTP_GET, handle_jpg_stream);
  server.begin();
  Serial.println("HTTP server started");
  Serial.print("Camera Stream Ready! Go to: http://");
  Serial.print(IP);
  Serial.println("/stream");
}

void loop() {
  uint8_t rc = ov5640.getFWStatus();
  server.handleClient();

  // 检查舵机状态并执行舵机旋转任务
  if (millis() - lastRotateTime >= rotateInterval) {
    rotateServo();
    lastRotateTime = millis();
  }

  // 打印数据
  if (millis() - lastPrintTime >= printInterval) {
    Serial.println("Printing data...");
    // 可以在这里添加其他需要打印的数据
    lastPrintTime = millis();
  }
}

void rotateServo() {
  switch (servoState) {
    case SERVO_IDLE:
      // 将舵机状态设置为旋转
      servoState = SERVO_ROTATING;
      // 将舵机旋转到 90 度
      servo.write(90);
      break;
    case SERVO_ROTATING:
      // 检查舵机是否已经到达目标位置
      if (millis() - lastRotateTime >= 1000) {
        // 将舵机状态设置为空闲
        servoState = SERVO_IDLE;
        // 将舵机旋转到 0 度
        servo.write(0);
      }
      break;
    default:
      break;
  }
}

void handle_jpg_stream() {
  WiFiClient client = server.client();
  camera_fb_t * fb = NULL;
  const char* part_buf[64];
  static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=frame";
  static const char* _STREAM_BOUNDARY = "\r\n--frame\r\n";
  static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, _STREAM_CONTENT_TYPE);

  while (true) {
    // 检查舵机状态并执行舵机旋转任务
    if (millis() - lastRotateTime >= rotateInterval) {
      rotateServo();
      lastRotateTime = millis();
    }

    if (millis() - lastPrintTime >= printInterval) {
      Serial.println("#1");
      // 可以在这里添加其他需要打印的数据
      lastPrintTime = millis();
    }

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
    snprintf((char *)part_buf, 64, _STREAM_PART, fb_len);
    server.sendContent((const char *)part_buf);
    server.sendContent((const char *)fb->buf, fb_len);
    esp_camera_fb_return(fb);

    if (!client.connected()) {
      break;
    }
  }
}


