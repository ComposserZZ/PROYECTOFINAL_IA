/* Includes ---------------------------------------------------------------- */
#include <composserzz-project-1_inferencing.h> //LIBRERIA MODELO DE LA IA 
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include "esp_camera.h"

#define CAMERA_MODEL_AI_THINKER // Has PSRAM

// Pines para señales al Arduino
#define PIN_50PESO  13
#define PIN_10PESO  12
#define PIN_5PESO   15
#define PIN_2PESO   2
#define PIN_1PESO   14


#if defined(CAMERA_MODEL_AI_THINKER)
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
#else
  #error "Camera model not selected"
#endif

#define EI_CAMERA_RAW_FRAME_BUFFER_COLS   320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS   240
#define EI_CAMERA_FRAME_BYTE_SIZE         3  // RGB888

static bool debug_nn = false;
static bool is_initialised = false;
uint8_t *snapshot_buf;

static camera_config_t camera_config = {
    .pin_pwdn       = PWDN_GPIO_NUM,
    .pin_reset      = RESET_GPIO_NUM,
    .pin_xclk       = XCLK_GPIO_NUM,
    .pin_sscb_sda   = SIOD_GPIO_NUM,
    .pin_sscb_scl   = SIOC_GPIO_NUM,
    .pin_d7         = Y9_GPIO_NUM,
    .pin_d6         = Y8_GPIO_NUM,
    .pin_d5         = Y7_GPIO_NUM,
    .pin_d4         = Y6_GPIO_NUM,
    .pin_d3         = Y5_GPIO_NUM,
    .pin_d2         = Y4_GPIO_NUM,
    .pin_d1         = Y3_GPIO_NUM,
    .pin_d0         = Y2_GPIO_NUM,
    .pin_vsync      = VSYNC_GPIO_NUM,
    .pin_href       = HREF_GPIO_NUM,
    .pin_pclk       = PCLK_GPIO_NUM,
    .xclk_freq_hz   = 20000000,
    .ledc_timer     = LEDC_TIMER_0,
    .ledc_channel   = LEDC_CHANNEL_0,
    .pixel_format   = PIXFORMAT_JPEG,
    .frame_size     = FRAMESIZE_QVGA,
    .jpeg_quality   = 12,
    .fb_count       = 1,
    .fb_location    = CAMERA_FB_IN_PSRAM,
    .grab_mode      = CAMERA_GRAB_WHEN_EMPTY,
};

bool ei_camera_init(void);
void ei_camera_deinit(void);
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf);
static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr);

void setup()
{
    Serial.begin(115200);
    while (!Serial);
    Serial.println("Edge Impulse Inferencing Demo");

    // Inicializar pines de salida para señal al Arduino
    pinMode(PIN_50PESO, OUTPUT);
    pinMode(PIN_10PESO, OUTPUT);
    pinMode(PIN_5PESO, OUTPUT);
    pinMode(PIN_2PESO, OUTPUT);
    pinMode(PIN_1PESO, OUTPUT);
    

    if (!ei_camera_init()) {
        ei_printf("Failed to initialize Camera!\r\n");
    }
    else {
        ei_printf("Camera initialized\r\n");
    }

    ei_printf("\nStarting continuous inference in 2 seconds...\n");
    ei_sleep(2000);
}

void loop()
{
    if (ei_sleep(5) != EI_IMPULSE_OK) {
        return;
    }

    snapshot_buf = (uint8_t*)malloc(
        EI_CAMERA_RAW_FRAME_BUFFER_COLS *
        EI_CAMERA_RAW_FRAME_BUFFER_ROWS *
        EI_CAMERA_FRAME_BYTE_SIZE
    );
    if (!snapshot_buf) {
        ei_printf("ERR: Failed to allocate snapshot buffer!\n");
        return;
    }

    ei::signal_t signal;
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data     = &ei_camera_get_data;

    if (!ei_camera_capture(
            (size_t)EI_CLASSIFIER_INPUT_WIDTH,
            (size_t)EI_CLASSIFIER_INPUT_HEIGHT,
            snapshot_buf
        )) {
        ei_printf("Failed to capture image\r\n");
        free(snapshot_buf);
        return;
    }

    ei_impulse_result_t result = { 0 };
    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, debug_nn);
    if (err != EI_IMPULSE_OK) {
        ei_printf("ERR: Failed to run classifier (%d)\n", err);
        free(snapshot_buf);
        return;
    }

    float valorMoneda = 0.0f;

#if EI_CLASSIFIER_OBJECT_DETECTION == 1
    float  max_val = 0.0f;
    int    max_ix  = -1;
    for (uint32_t i = 0; i < result.bounding_boxes_count; i++) {
        ei_impulse_result_bounding_box_t bb = result.bounding_boxes[i];
        if (bb.value > max_val) {
            max_val = bb.value;
            max_ix  = i;
        }
    }

    if (max_ix >= 0) {
        ei_impulse_result_bounding_box_t bb = result.bounding_boxes[max_ix];
        const char *label = bb.label;

        if (strcmp(label, "10PESO") == 0) {
            valorMoneda = 10.0f;
        }
        else if (strcmp(label, "5PESO") == 0) {
            valorMoneda = 5.0f;
        }
        else if (strcmp(label, "2PESO") == 0) {
            valorMoneda = 2.0f;
        }
        else if (strcmp(label, "1PESO") == 0) {
            valorMoneda = 1.0f;
        }
        else if (strcmp(label, "50PESO") == 0) {
            valorMoneda = 50.0f;
        }

        ei_printf("Moneda detectada: %s (%.2f%%)\r\n", label, bb.value * 100.0f);
        ei_printf("valorMoneda = %.1f\r\n", valorMoneda);
    }
#endif

    // Señal al Arduino: apagar todos los pines primero
    digitalWrite(PIN_50PESO, LOW);
    digitalWrite(PIN_10PESO, LOW);
    digitalWrite(PIN_5PESO, LOW);
    digitalWrite(PIN_2PESO, LOW);
    digitalWrite(PIN_1PESO, LOW);
   

    // Encender solo el correspondiente
    if (valorMoneda == 50.0f) {
        digitalWrite(PIN_50PESO, HIGH);
    } else if (valorMoneda == 10.0f) {
        digitalWrite(PIN_10PESO, HIGH);
    } else if (valorMoneda == 5.0f) {
        digitalWrite(PIN_5PESO, HIGH);
    } else if (valorMoneda == 2.0f) {
        digitalWrite(PIN_2PESO, HIGH);
    } else if (valorMoneda == 1.0f) {
        digitalWrite(PIN_1PESO, HIGH);
    }

#if EI_CLASSIFIER_HAS_ANOMALY
    ei_printf("Anomaly prediction: %.3f\r\n", result.anomaly);
#endif

    free(snapshot_buf);
}

bool ei_camera_init(void)
{
    if (is_initialised) return true;
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        return false;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s->id.PID == OV3660_PID) {
        s->set_vflip(s, 1);
        s->set_brightness(s, 1);
        s->set_saturation(s, 0);
    }

    is_initialised = true;
    return true;
}

void ei_camera_deinit(void)
{
    esp_camera_deinit();
    is_initialised = false;
}

bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf)
{
    if (!is_initialised) return false;

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) return false;

    bool converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, snapshot_buf);
    esp_camera_fb_return(fb);
    if (!converted) return false;

    if ((img_width != EI_CAMERA_RAW_FRAME_BUFFER_COLS) || (img_height != EI_CAMERA_RAW_FRAME_BUFFER_ROWS)) {
        ei::image::processing::crop_and_interpolate_rgb888(
            out_buf,
            EI_CAMERA_RAW_FRAME_BUFFER_COLS,
            EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
            out_buf,
            img_width,
            img_height
        );
    }

    return true;
}

static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr)
{
    size_t pixel_ix = offset * 3;
    size_t pixels_left = length;
    size_t out_ix = 0;

    while (pixels_left != 0) {
        uint8_t r = snapshot_buf[pixel_ix + 2];
        uint8_t g = snapshot_buf[pixel_ix + 1];
        uint8_t b = snapshot_buf[pixel_ix + 0];

        out_ptr[out_ix] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        out_ix++;
        pixel_ix += 3;
        pixels_left--;
    }
    return 0;
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_CAMERA
  #error "Invalid model for current sensor"
#endif