#include <SPI.h>
#include <LoRa.h>

#define LORA_SS 5
#define LORA_RST 14
#define LORA_DIO0 2

uint8_t key[16] = "CanSatKey123456";  // Must match transmitter
uint32_t packetCounter = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial);

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
    Serial.println("[OK] LoRa receiver initialized with 1 km settings");
    Serial.println("Waiting for data...");
}

void loop() {
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
        Serial.println("\n📡 Packet Received!");
        
        String encryptedData = "";
        while (LoRa.available() && encryptedData.length() < packetSize - 4) {
            encryptedData += (char)LoRa.read();
        }
        
        // Read CRC32 (last 4 bytes)
        uint8_t crcBytes[4];
        for (int i = 0; i < 4; i++) {
            crcBytes[i] = LoRa.read();
        }
        uint32_t receivedCRC = *(uint32_t*)crcBytes;
        
        // Verify CRC32
        uint8_t dataBytes[encryptedData.length() + 1];
        encryptedData.getBytes(dataBytes, encryptedData.length() + 1);
        uint32_t calculatedCRC = calculateCRC32(dataBytes, encryptedData.length());
        
        if (receivedCRC == calculatedCRC) {
            Serial.println("🔒 CRC32 Valid: " + String(receivedCRC, HEX));
            
            // Decrypt with XOR
            String decryptedData = applyXOR(encryptedData, packetCounter);
            
            Serial.println("🔹 Decrypted Data: " + decryptedData);
            Serial.println("🔹 RSSI: " + String(LoRa.packetRssi()));
            Serial.println("🔹 SNR: " + String(LoRa.packetSnr()));
            packetCounter++;
        } else {
            Serial.println("⚠️ CRC32 Mismatch! Received: " + String(receivedCRC, HEX) + ", Calculated: " + String(calculatedCRC, HEX));
        }
    }
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