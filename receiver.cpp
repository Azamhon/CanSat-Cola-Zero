#include <SPI.h>
#include <LoRa.h>
#include <AES.h>
#include <CRC32.h>

#define LORA_SS 5
#define LORA_RST 14
#define LORA_DIO0 2

AES128 aes;
CRC32 crc;

uint8_t key[16] = {0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C}; // AES key

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

    aes.setKey(key, 16);

    Serial.println("[OK] LoRa receiver initialized with 1 km settings");
    Serial.println("Waiting for data...");
}

void loop() {
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
        Serial.println("\n📡 Packet Received!");
        
        uint8_t encryptedData[128];
        uint32_t receivedChecksum;
        
        LoRa.readBytes(encryptedData, sizeof(encryptedData));
        LoRa.readBytes((uint8_t*)&receivedChecksum, sizeof(receivedChecksum));
        
        // CRC32 check
        crc.reset();
        crc.update(encryptedData, sizeof(encryptedData));
        uint32_t calculatedChecksum = crc.finalize();
        
        if (calculatedChecksum != receivedChecksum) {
            Serial.println("[Error] Data integrity check failed!");
            return;
        }
        
        // AES Decryption
        uint8_t decryptedData[128];
        aes.decryptBlock(decryptedData, encryptedData);
        
        // XOR De-obfuscation
        String obfuscatedData = String((char*)decryptedData);
        String originalData = xorObfuscate(obfuscatedData, 0xAA); // 0xAA is the XOR key
        
        Serial.println("🔹 Decrypted Data: " + originalData);
        Serial.println("🔹 RSSI: " + String(LoRa.packetRssi()));
        Serial.println("🔹 SNR: " + String(LoRa.packetSnr()));
    }
}

String xorObfuscate(String data, uint8_t key) {
    String result = data;
    for (int i = 0; i < data.length(); i++) {
        result[i] = data[i] ^ key;
    }
    return result;
}