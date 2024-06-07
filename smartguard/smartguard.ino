#include <fall_detection_model.h>
#include <Arduino_LSM9DS1.h>
#include <ArduinoBLE.h>

// Setup BLE service and characteristics
BLEService fallDetectionService("fff0");
BLEIntCharacteristic fallAlertCharacteristic("fff1", BLERead | BLEBroadcast);
const uint8_t advertisingData[] = {0x02, 0x01, 0x06, 0x09, 0xff, 0x01, 0x01, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
BLEAdvertisingData adData;

// Define LED pins
#define RED_LED 22
#define BLUE_LED 24
#define GREEN_LED 23
#define POWER_LED 25

int fallCounter = 0;
String identifier = "Elderly";
int delayTime = 5;
int secondsCounter = 0;

// Constants for acceleration conversion and maximum acceleration
#define GRAVITY_MS2 9.80665f
#define MAX_RANGE 2.0f 

// Inference variables
static bool enableDebug = true;
static uint32_t inferenceInterval = 2000;
static rtos::Thread inferenceThread(osPriorityLow);
static float sensorDataBuffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = {0};
static float processedDataBuffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

// Forward declaration for background inference function
void performInferenceInBackground();

/**
 * @brief      Setup function for Arduino
 */
void setup() {

    pinMode(RED_LED, OUTPUT);
    pinMode(BLUE_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);
    pinMode(POWER_LED, OUTPUT);

    Serial.begin(115200);
    performLightShow();

    delay(5000);

    Serial.println("Edge Impulse Inferencing Demo");

    if (!IMU.begin()) {
        Serial.println("Failed to initialize IMU!");
    } else {
        Serial.println("IMU initialized");
    }

    if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME != 3) {
        Serial.println("Error: Expecting 3 sensor axes data per frame");
        return;
    }

    if (!BLE.begin()) {
        Serial.println("Failed to initialize BLE!");
        while (1);
    }

    fallDetectionService.addCharacteristic(fallAlertCharacteristic);
    BLE.addService(fallDetectionService);

    // Setup BLE advertising data
    BLEAdvertisingData advertising;
    advertising.setRawData(advertisingData, sizeof(advertisingData));
    BLE.setAdvertisingData(advertising);

    inferenceThread.start(mbed::callback(&performInferenceInBackground));
}

void performLightShow() {

    digitalWrite(RED_LED, HIGH);
    delay(500);
    digitalWrite(RED_LED, LOW);

    digitalWrite(BLUE_LED, HIGH);
    delay(500);
    digitalWrite(BLUE_LED, LOW);

    digitalWrite(GREEN_LED, HIGH);
    delay(500);
}

void activateRedLights() {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
}

void deactivateRedLights() {
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, HIGH);
}

void startFallAlert(String fallCode) {

    Serial.println("Advertising...");

    char buffer[50];
    fallCode.toCharArray(buffer, 50);

    adData.setLocalName(buffer);
    BLE.setScanResponseData(adData);
    BLE.advertise();
}

void stopAdvertising() {
    Serial.println("Stop advertising...");
    BLE.stopAdvertise();
}

float getSign(float value) {
    return (value >= 0.0) ? 1.0 : -1.0;
}

void performInferenceInBackground() {
    delay((EI_CLASSIFIER_INTERVAL_MS * EI_CLASSIFIER_RAW_SAMPLE_COUNT) + 100);

    ei_classifier_smooth_t smooth;
    ei_classifier_smooth_init(&smooth, 10, 7, 0.8, 0.3);

    while (1) {
        memcpy(processedDataBuffer, sensorDataBuffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE * sizeof(float));

        signal_t signal;
        int error = numpy::signal_from_buffer(processedDataBuffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
        if (error != 0) {
            Serial.printf("Error creating signal from buffer: %d\n", error);
            return;
        }

        ei_impulse_result_t result;
        error = run_classifier(&signal, &result, enableDebug);
        if (error != EI_IMPULSE_OK) {
            Serial.printf("Error running classifier: %d\n", error);
            return;
        }

        Serial.print("Predictions ");
        const char *prediction = ei_classifier_smooth_update(&smooth, &result);
        Serial.print(prediction);

        Serial.print(" [ ");
        for (size_t ix = 0; ix < smooth.count_size; ix++) {
            Serial.printf("%u", smooth.count[ix]);
            if (ix != smooth.count_size - 1) {
                Serial.print(", ");
            }
        }
        Serial.println("]");

        for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
            Serial.printf("    %s: %.5f\n", result.classification[ix].label, result.classification[ix].value);

            if (result.classification[ix].label == "Fall" && result.classification[ix].value > 0.7) {
                fallCounter++;
                startFallAlert("Fall-" + identifier + "-" + String(fallCounter));
                activateRedLights();
            }

            if (result.classification[ix].label == "Stand" && result.classification[ix].value > 0.7) {
                deactivateRedLights();
            }
        }

        delay(inferenceInterval);
    }

    ei_classifier_smooth_free(&smooth);
}

void loop() {
    // Main loop actions
    while (1) {

        fallCounter++;

        BLE.poll();

        uint64_t next_tick = micros() + (EI_CLASSIFIER_INTERVAL_MS * 1000);
        numpy::roll(sensorDataBuffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, -3);
        IMU.readAcceleration(
            sensorDataBuffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 3],
            sensorDataBuffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 2],
            sensorDataBuffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 1]);

        for (int i = 0; i < 3; i++) {
            float &value = sensorDataBuffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 3 + i];
            if (fabs(value) > MAX_RANGE) {
                value = getSign(value) * MAX_RANGE;
            }
        }

        for (int i = 0; i < 3; i++) {
            sensorDataBuffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 3 + i] *= GRAVITY_MS2;
        }

        uint64_t time_to_wait = next_tick - micros();
        delay((int)floor((float)time_to_wait / 1000.0f));
        delayMicroseconds(time_to_wait % 1000);
    }
}
