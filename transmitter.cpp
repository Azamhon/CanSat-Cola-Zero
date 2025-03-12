#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <MPU6050.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>

#define LORA_SS 5
#define LORA_RST 14
#define LORA_DIO0 2
#define MQ135_PIN 34
#define GPS_RX 16
#define GPS_TX 17

Adafruit_BME280 bme;
MPU6050 mpu;
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);

float filterAx = 0, filterAy = 0, filterAz = 0;
const float alpha = 0.1;  // Smoothing factor for acceleration

uint8_t key[16] = "CanSatKey123456";  // 16-byte XOR key
uint32_t packetCounter = 0;  // Dynamic counter for XOR

void setup() {
    Serial.begin(115200);
    gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
    
    SPI.begin();
    LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
    if (!LoRa.begin(433E6)) {
        Serial.println("[Error] LoRa initialization failed!");
        while (1);
    }
    
    LoRa.setSpreadingFactor(10);
    LoRa.setSignalBandwidth(250E3);
    LoRa.setCodingRate4(6);
    LoRa.setTxPower(17, PA_OUTPUT_PA_BOOST_PIN);
    LoRa.enableCrc();
    Serial.println("[OK] LoRa initialized with 1 km optimized settings.");
    
    if (!bme.begin(0x76)) {
        Serial.println("[Error] BME280 not found!");
        while (1);
    }
    Serial.println("[OK] BME280 initialized");
    
    Wire.begin();
    mpu.initialize();
    if (!mpu.testConnection()) {
        Serial.println("[Error] MPU6050 connection failed!");
        while (1);
    }
    Serial.println("[OK] MPU6050 initialized");
    
    Serial.println("CanSat Transmitter Ready!");
}

void loop() {
    float temperature = bme.readTemperature();
    float pressure = bme.readPressure() / 100.0F;
    float humidity = bme.readHumidity();
    float altitude = bme.readAltitude(1013.25);
    
    int16_t ax, ay, az, gx, gy, gz;
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    
    float ax_mps2 = (ax / 16384.0) * 9.81;
    float ay_mps2 = (ay / 16384.0) * 9.81;
    float az_mps2 = (az / 16384.0) * 9.81;
    
    filterAx = alpha * ax_mps2 + (1 - alpha) * filterAx;
    filterAy = alpha * ay_mps2 + (1 - alpha) * filterAy;
    filterAz = alpha * az_mps2 + (1 - alpha) * filterAz;
    
    float airQualityRaw = analogRead(MQ135_PIN);
    float airQualityPPM = map(airQualityRaw, 0, 4095, 400, 5000);
    String airQualityStatus = classifyAirQuality(airQualityPPM);
    
    float latitude = 0.0, longitude = 0.0;
    updateGPS(latitude, longitude);
    
    String dataPacket = String(temperature) + "," + String(pressure) + "," + String(altitude) + "," +
                       String(humidity) + "," + String(filterAx) + "," + String(filterAy) + "," + String(filterAz) + "," +
                       String(airQualityPPM) + "(" + airQualityStatus + ")," + String(latitude, 6) + "," + String(longitude, 6);
    
    // Step 1: XOR Encryption with dynamic key
    String encryptedData = applyXOR(dataPacket, packetCounter);
    
    // Step 2: CRC32 for integrity (only over encrypted data)
    uint8_t dataBytes[encryptedData.length() + 1];
    encryptedData.getBytes(dataBytes, encryptedData.length() + 1);
    uint32_t crc = calculateCRC32(dataBytes, encryptedData.length());
    
    // Step 3: Send packetCounter + encrypted data + CRC32
    LoRa.beginPacket();
    LoRa.write((uint8_t*)&packetCounter, sizeof(packetCounter));  // 4-byte packetCounter
    LoRa.print(encryptedData);                                    // Encrypted data
    LoRa.write((uint8_t*)&crc, sizeof(crc));                      // 4-byte CRC32
    LoRa.endPacket();
    
    Serial.println("Sending encrypted packet (len=" + String(encryptedData.length()) + ") with Counter: " + String(packetCounter) + ", CRC32: " + String(crc, HEX));
    packetCounter++;
    
    delay(1000);  // Send every 1 second
}

String applyXOR(String data, uint32_t counter) {
    String result = data;
    for (int i = 0; i < result.length(); i++) {
        uint8_t keyByte = key[i % 16] ^ (counter & 0xFF);  // Dynamic key per packet
        result[i] = result[i] ^ keyByte;
    }
    return result;
}

uint32_t calculateCRC32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

void updateGPS(float &lat, float &lng) {
    while (gpsSerial.available()) {
        gps.encode(gpsSerial.read());
    }
    if (gps.location.isValid()) {
        lat = gps.location.lat();
        lng = gps.location.lng();
    } else {
        lat = 0.0;
        lng = 0.0;
        Serial.println("[Warning] No valid GPS data!");
    }
}

String classifyAirQuality(float ppm) {
    if (ppm < 500) return "Good";
    else if (ppm < 1000) return "Moderate";
    else if (ppm < 2000) return "Unhealthy";
    else return "Hazardous";
}