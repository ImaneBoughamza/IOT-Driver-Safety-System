// ============================================================
// Driver Safety Monitoring System — ESP32-CAM Firmware
// Lane Departure Detection + Photon Communication
// Board: AI Thinker ESP32-CAM
// ============================================================
#include "esp_camera.h"
#include "img_converters.h"
#include <WiFi.h>
#include <HTTPClient.h>

// Camera pins (AI-Thinker ESP32-CAM)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

const char* ssid     = "YOUR_HOTSPOT";
const char* password = "YOUR_WIFI_PASSWORD";

#define BRIGHTNESS_THRESHOLD 180
#define LANE_PIXEL_THRESHOLD 500
#define PHOTON_DEVICE_ID     "YOUR_PHOTON_DEVICE_ID"
#define PARTICLE_TOKEN       "YOUR_ACCESS_TOKEN"
#define LANE_SEND_INTERVAL   3000   // Send lane status every 3 seconds

WiFiServer server(80);
bool lastLaneStatus         = false;
unsigned long lastSendTime  = 0;

// Send lane departure status to Photon via Particle Cloud
void sendLaneStatus(bool laneDeparture) {
    if (millis() - lastSendTime < LANE_SEND_INTERVAL) return;
    lastSendTime = millis();

    HTTPClient http;
    String url = "https://api.particle.io/v1/devices/"
        + String(PHOTON_DEVICE_ID) + "/laneAlert";

    http.begin(url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String body = "access_token=" + String(PARTICLE_TOKEN)
        + "&arg=" + (laneDeparture ? "1" : "0");

    int httpCode = http.POST(body);

    if (httpCode > 0) {
        Serial.printf("Lane status sent: %s (HTTP %d)\n",
            laneDeparture ? "DEPARTURE" : "OK", httpCode);
    } else {
        Serial.printf("Failed to send lane status: %s\n",
            http.errorToString(httpCode).c_str());
    }

    http.end();
}

void setup() {
    Serial.begin(115200);

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0       = Y2_GPIO_NUM;
    config.pin_d1       = Y3_GPIO_NUM;
    config.pin_d2       = Y4_GPIO_NUM;
    config.pin_d3       = Y5_GPIO_NUM;
    config.pin_d4       = Y6_GPIO_NUM;
    config.pin_d5       = Y7_GPIO_NUM;
    config.pin_d6       = Y8_GPIO_NUM;
    config.pin_d7       = Y9_GPIO_NUM;
    config.pin_xclk     = XCLK_GPIO_NUM;
    config.pin_pclk     = PCLK_GPIO_NUM;
    config.pin_vsync    = VSYNC_GPIO_NUM;
    config.pin_href     = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn     = PWDN_GPIO_NUM;
    config.pin_reset    = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_GRAYSCALE;
    config.frame_size   = FRAMESIZE_QVGA; // 320x240
    config.fb_count     = 2;

    if (esp_camera_init(&config) != ESP_OK) {
        Serial.println("Camera init failed");
        return;
    }

    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected!");
    Serial.println("Stream: http://" + WiFi.localIP().toString());

    server.begin();
}

void handleClient(WiFiClient client) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
    client.println("Access-Control-Allow-Origin: *");
    client.println();

    while (client.connected()) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) { delay(100); continue; }

        // --- Lane Detection ---
        int w = fb->width, h = fb->height;
        int xStart = w / 3, xEnd = (w * 2) / 3;
        int yStart = h / 2;
        int whitePixels = 0, totalPixels = 0;

        for (int y = yStart; y < h; y++) {
            for (int x = xStart; x < xEnd; x++) {
                if (fb->buf[y * w + x] > BRIGHTNESS_THRESHOLD) whitePixels++;
                totalPixels++;
            }
        }

        float ratio = (float)whitePixels / totalPixels * 100.0;
        bool lane = whitePixels > LANE_PIXEL_THRESHOLD;
        bool laneDeparture = !lane; // No lane detected = departure

        Serial.printf("%s | White pixels: %d | Ratio: %.1f%%\n",
            lane ? "LANE DETECTED" : "LANE DEPARTURE",
            whitePixels, ratio);

        // --- Send lane status to Photon ---
        sendLaneStatus(laneDeparture);

        // --- Convert grayscale to JPEG for stream ---
        uint8_t *jpg_buf = NULL;
        size_t jpg_len = 0;
        bool ok = fmt2jpg(fb->buf, fb->len, w, h,
                           PIXFORMAT_GRAYSCALE, 80, &jpg_buf, &jpg_len);
        esp_camera_fb_return(fb);

        if (ok) {
            client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", jpg_len);
            client.write(jpg_buf, jpg_len);
            client.println();
            free(jpg_buf);
        }

        delay(100);
    }
}

void loop() {
    WiFiClient client = server.available();
    if (client) handleClient(client);

    // Still send lane status even without a stream viewer
    if (!server.available()) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            int w = fb->width, h = fb->height;
            int xStart = w / 3, xEnd = (w * 2) / 3;
            int yStart = h / 2;
            int whitePixels = 0;

            for (int y = yStart; y < h; y++) {
                for (int x = xStart; x < xEnd; x++) {
                    if (fb->buf[y * w + x] > BRIGHTNESS_THRESHOLD) whitePixels++;
                }
            }

            bool laneDeparture = (whitePixels <= LANE_PIXEL_THRESHOLD);
            sendLaneStatus(laneDeparture);
            esp_camera_fb_return(fb);
        }
        delay(100);
    }
}
