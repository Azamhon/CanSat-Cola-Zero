import serial
import csv
import time
from datetime import datetime

# Serial port configuration
SERIAL_PORT = 'COM7'  # Update as needed (e.g., '/dev/ttyUSB0' for Linux)
BAUD_RATE = 115200
CSV_FILE = 'cansat_data.csv'

# CSV Header
HEADER = [
    "Timestamp", "Temperature (°C)", "Pressure (hPa)", "Altitude (m)",
    "Humidity (%)", "Acceleration_X (m/s²)", "Acceleration_Y (m/s²)", "Acceleration_Z (m/s²)",
    "Velocity_X (m/s)", "Velocity_Y (m/s)", "Velocity_Z (m/s)", "AirQuality", "Latitude", "Longitude"
]

# Initialize serial connection
try:
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    print(f"[OK] Connected to {SERIAL_PORT} at {BAUD_RATE} baud.")
except Exception as e:
    print(f"[Error] Failed to connect: {e}")
    exit()

# Create CSV file with header if it doesn’t exist
try:
    with open(CSV_FILE, mode='w', newline='', encoding='utf-8') as file:
        writer = csv.writer(file)
        writer.writerow(HEADER)
    print(f"[OK] CSV file '{CSV_FILE}' created.")
except Exception as e:
    print(f"[Error] CSV file creation failed: {e}")
    exit()

last_altitude = None
last_time = None
velocity_x = 0.0
velocity_y = 0.0
velocity_z = 0.0

# Velocity integration (approximation)
def update_velocity(acc, last_velocity, delta_time):
    return round(last_velocity + acc * delta_time, 2)

def parse_data(data):
    global last_altitude, last_time, velocity_x, velocity_y, velocity_z
    try:
        # Remove the "🔹 Decrypted Data: " prefix and strip whitespace
        values = data.replace("🔹 Decrypted Data: ", "").strip().split(',')
        if len(values) >= 10:  # Ensure all values are present
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            temperature = float(values[0])
            pressure = float(values[1])
            altitude = float(values[2])
            humidity = float(values[3])
            acc_x = float(values[4])
            acc_y = float(values[5])
            acc_z = float(values[6])
            air_quality = values[7]  # Keep as string (e.g., "450(Good)")
            latitude = float(values[8])
            longitude = float(values[9])
            
            current_time = time.time()
            if last_time is not None:
                delta_time = current_time - last_time
                velocity_x = update_velocity(acc_x, velocity_x, delta_time)
                velocity_y = update_velocity(acc_y, velocity_y, delta_time)
                if last_altitude is not None:
                    velocity_z = round((altitude - last_altitude) / delta_time, 2)
            
            last_time = current_time
            last_altitude = altitude
            
            return [
                timestamp, temperature, pressure, altitude, humidity,
                acc_x, acc_y, acc_z, velocity_x, velocity_y, velocity_z,
                air_quality, latitude, longitude
            ]
        else:
            print(f"[Warning] Invalid format: {data}")
            return None
    except Exception as e:
        print(f"[Error] Parsing failed: {e}")
        return None

# Main loop
print("[OK] Listening for CanSat data...")
try:
    buffer = ""  # Buffer to handle multiline input
    while True:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            buffer += line + "\n"
            
            # Check for the decrypted data line
            if "🔹 Decrypted Data: " in line:
                print("\n📡 Packet Received!")  # Mimic receiver output
                data = line
                parsed_data = parse_data(data)
                if parsed_data:
                    with open(CSV_FILE, mode='a', newline='') as file:
                        writer = csv.writer(file)
                        writer.writerow(parsed_data)
                    print(f"🔹 Decrypted Data: {','.join(map(str, parsed_data[1:]))}")
                    print(f"🔹 Timestamp: {parsed_data[0]}")
                
                # Print additional lines (RSSI, SNR, Counter) from buffer if available
                buffer_lines = buffer.split('\n')
                for buf_line in buffer_lines:
                    if "🔹 RSSI: " in buf_line:
                        print(buf_line)
                    elif "🔹 SNR: " in buf_line:
                        print(buf_line)
                    elif "🔹 Packet Counter: " in buf_line:
                        print(buf_line)
                buffer = ""  # Clear buffer after processing a packet
            
            elif "⚠️ CRC32 Mismatch" in line or "🔒 CRC32 Valid" in line:
                print(line)

        time.sleep(0.1)
except KeyboardInterrupt:
    print("\n[OK] Stopped.")
except Exception as e:
    print(f"[Error] {e}")
finally:
    ser.close()
    print("[OK] Serial closed.")