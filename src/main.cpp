
#include "esp_camera.h"
#include <WiFi.h>
#include "Drv/ESP32_OV5640_AF.h"
#include <WebServer.h>

#include <Arduino.h>
#include <ESP32Servo.h>

#define SERVO_PIN 13 // 连接舵机的引脚

// 创建 Servo 对象
Servo servo;


// ESP32 AI-THINKER Board
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

void rotateServo() {
  // 将舵机旋转到 0 度
  servo.write(0);
  delay(1000); // 等待1秒钟

  // 将舵机旋转到 90 度
  servo.write(90);
  delay(1000); // 等待1秒钟
}


// const char* ssid = "huchuang";
// const char* password = "83754648";

const char* ssid = "iPhone 14 plus";
const char* password = "mwrobot666";

OV5640 ov5640 = OV5640();
WebServer server(80);

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

void setup() {
  Serial.begin(115200);

  // 设置舵机连接的引脚
  servo.attach(SERVO_PIN);

  // 舵机归位
  servo.write(0);
  delay(1000);

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

//frame_size = FRAMESIZE_96X96,      // 96x96
//frame_size = FRAMESIZE_QQVGA,      // 160x120
//frame_size = FRAMESIZE_QCIF,       // 176x144
//frame_size = FRAMESIZE_HQVGA,      // 240x176
//frame_size = FRAMESIZE_240X240,    // 240x240
//frame_size = FRAMESIZE_QVGA,       // 320x240
//frame_size = FRAMESIZE_CIF,        // 400x296
//frame_size = FRAMESIZE_HVGA,       // 480x320
//frame_size = FRAMESIZE_VGA,        // 640x480
//frame_size = FRAMESIZE_SVGA,       // 800x600
//frame_size = FRAMESIZE_XGA,        // 1024x768
//frame_size = FRAMESIZE_HD,         // 1280x720
//frame_size = FRAMESIZE_SXGA,       // 1280x1024
//frame_size = FRAMESIZE_UXGA        // 1600x1200


  config.frame_size = FRAMESIZE_UXGA; 
  config.jpeg_quality = 10;
  config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t* sensor = esp_camera_sensor_get();
  ov5640.start(sensor);

  if (ov5640.focusInit() == 0) {
    Serial.println("OV5640_Focus_Init Successful!");
  }

  if (ov5640.autoFocusMode() == 0) {
    Serial.println("OV5640_Auto_Focus Successful!");
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  server.on("/stream", HTTP_GET, handle_jpg_stream);
  server.begin();
  Serial.println("HTTP server started");
  Serial.print("Camera Stream Ready! Go to: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/stream");
}

void loop() {
  static unsigned long lastRotateTime = 0;
  const unsigned long rotateInterval = 2000; // 舵机旋转间隔时间，单位：毫秒

  uint8_t rc = ov5640.getFWStatus();
  // Serial.printf("FW_STATUS = 0x%x\n", rc);

  switch(rc) {
    case -1:
      Serial.println("Check your OV5640");
      break;
    case FW_STATUS_S_FOCUSED:
      Serial.println("Focused!");
      break;
    case FW_STATUS_S_FOCUSING:
      Serial.println("Focusing!");
      break;
    default:
      break;
  }

  server.handleClient();

  // 每隔一段时间执行舵机旋转任务
  if (millis() - lastRotateTime >= rotateInterval) {
    rotateServo();
    lastRotateTime = millis();
  }

  // rotateServo();
}



