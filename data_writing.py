import serial
import csv
import time
from datetime import datetime

# Serial port configuration
SERIAL_PORT = 'COM8'  # Update as needed
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
        values = data.strip().split(',')
        if len(values) >= 10:  # Ensure all values are present
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            altitude = float(values[2])
            humidity = float(values[3])
            acc_x = float(values[4])
            acc_y = float(values[5])
            acc_z = float(values[6])
            current_time = time.time()
            
            if last_time is not None:
                delta_time = current_time - last_time
                velocity_x = update_velocity(acc_x, velocity_x, delta_time)
                velocity_y = update_velocity(acc_y, velocity_y, delta_time)
                if last_altitude is not None:
                    velocity_z = round((altitude - last_altitude) / delta_time, 2)
            
            last_time = current_time
            last_altitude = altitude
            
            return [timestamp] + values[:3] + [humidity] + values[4:7] + [velocity_x, velocity_y, velocity_z] + values[7:]
        else:
            print(f"[Warning] Invalid format: {data}")
            return None
    except Exception as e:
        print(f"[Error] Parsing failed: {e}")
        return None

# Main loop
print("[OK] Listening for CanSat data...")
try:
    while True:
        if ser.in_waiting > 0:
            data = ser.readline().decode('utf-8').strip()
            
            if not data.startswith("Received:"):
                continue
            
            data = data.replace("Received: ", "").strip()
            print(f"Received: {data}")

            parsed_data = parse_data(data)
            if parsed_data:
                with open(CSV_FILE, mode='a', newline='') as file:
                    writer = csv.writer(file)
                    writer.writerow(parsed_data)
                print(f"[OK] Data logged: {parsed_data}")

        time.sleep(0.1)
except KeyboardInterrupt:
    print("\n[OK] Stopped.")
except Exception as e:
    print(f"[Error] {e}")
finally:
    ser.close()
    print("[OK] Serial closed.")
