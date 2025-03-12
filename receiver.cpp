#include <SPI.h>
#include <LoRa.h>

#define LORA_SS 5
#define LORA_RST 14
#define LORA_DIO0 2

uint8_t key[16] = "CanSatKey123456";  // Must match transmitter

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
        
        // Step 1: Read packetCounter (first 4 bytes)
        uint8_t counterBytes[4];
        for (int i = 0; i < 4; i++) {
            counterBytes[i] = LoRa.read();
        }
        uint32_t receivedCounter = *(uint32_t*)counterBytes;
        
        // Step 2: Read encrypted data
        String encryptedData = "";
        while (LoRa.available() && encryptedData.length() < packetSize - 8) {  // Adjust for 4-byte counter + 4-byte CRC
            encryptedData += (char)LoRa.read();
        }
        
        // Step 3: Read CRC32 (last 4 bytes)
        uint8_t crcBytes[4];
        for (int i = 0; i < 4; i++) {
            crcBytes[i] = LoRa.read();
        }
        uint32_t receivedCRC = *(uint32_t*)crcBytes;
        
        // Step 4: Verify CRC32 (only over encrypted data)
        uint8_t dataBytes[encryptedData.length() + 1];
        encryptedData.getBytes(dataBytes, encryptedData.length() + 1);
        uint32_t calculatedCRC = calculateCRC32(dataBytes, encryptedData.length());
        
        if (receivedCRC == calculatedCRC) {
            Serial.println("🔒 CRC32 Valid: " + String(receivedCRC, HEX));
            
            // Step 5: Decrypt using received packetCounter
            String decryptedData = applyXOR(encryptedData, receivedCounter);
            
            Serial.println("🔹 Decrypted Data: " + decryptedData);
            Serial.println("🔹 Packet Counter: " + String(receivedCounter));
            Serial.println("🔹 RSSI: " + String(LoRa.packetRssi()));
            Serial.println("🔹 SNR: " + String(LoRa.packetSnr()));
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