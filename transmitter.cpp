#include <SPI.h>
#include <LoRa.h> 
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <MPU6050.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <AES.h>
#include <ChaCha.h>
#include <CRC32.h>

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
const float alpha = 0.1; // Smoothing factor for acceleration

AES128 aes;
ChaCha chacha;
CRC32 crc;

uint8_t key[16] = {0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C}; // AES key
uint8_t nonce[8] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}; // ChaCha nonce

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
    
    aes.setKey(key, 16);
    chacha.setKey(key, 16);
    chacha.setIV(nonce, 8);
    
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
                        String(airQualityPPM) + "(" + airQualityStatus + ")," + String(latitude, 6) + "," + String(longitude, 6) + "#";
    
    // XOR Obfuscation
    String obfuscatedData = xorObfuscate(dataPacket, 0xAA); // 0xAA is the XOR key
    
    // AES Encryption
    uint8_t encryptedData[128];
    aes.encryptBlock(encryptedData, (uint8_t*)obfuscatedData.c_str());
    
    // CRC32 for integrity
    crc.reset();
    crc.update(encryptedData, sizeof(encryptedData));
    uint32_t checksum = crc.finalize();
    
    // Send data
    LoRa.beginPacket();
    LoRa.write(encryptedData, sizeof(encryptedData));
    LoRa.write((uint8_t*)&checksum, sizeof(checksum));
    LoRa.endPacket();
    
    Serial.println("Sending encrypted data...");
    delay(1000);  // Send data every 1 second
}

String xorObfuscate(String data, uint8_t key) {
    String result = data;
    for (int i = 0; i < data.length(); i++) {
        result[i] = data[i] ^ key;
    }
    return result;
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