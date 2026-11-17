/*
 * Praktikum Sensor Digital - Pembanding IMU 6-DOF vs 9-DOF Razor IMU
 * IMU 1 (MPU-6050)  : I2C (A4=SDA, A5=SCL)
 * IMU 2 (Razor 9DOF): SoftwareSerial (Pin 10=RX, Pin 11=TX)
 */

#include <Wire.h>
#include <SoftwareSerial.h>

SoftwareSerial razorSerial(10, 11); // RX = Pin 10 (ke TX Razor), TX = Pin 11

const int MPU_ADDR = 0x68;

// Data IMU 1 (MPU-6050 / 6-DOF)
float roll_6dof = 0, pitch_6dof = 0, yaw_6dof = 0;
float gz_offset = 0;
unsigned long prevTime = 0;

// Data IMU 2 (Razor / 9-DOF)
float yaw_9dof = 0, pitch_9dof = 0, roll_9dof = 0;
String razorBuffer = "";

// Inisialisasi MPU-6050
void initMPU() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); // PWR_MGMT_1
  Wire.write(0x00); // Bangunkan MPU-6050 dari mode sleep
  Wire.endTransmission(true);
}

// Kalibrasi Offset Gyro Z saat Diam
void calibrateGyroZ() {
  long sumGz = 0;
  for (int i = 0; i < 500; i++) {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x47); // Register GYRO_ZOUT_H
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDR, 2, true);
    
    if (Wire.available() >= 2) {
      uint8_t h = Wire.read();
      uint8_t l = Wire.read();
      sumGz += (int16_t)(h << 8 | l);
    }
    delay(2);
  }
  gz_offset = (sumGz / 500.0) / 131.0;
}

// Membaca MPU-6050 dengan penanganan byte terurut
void readMPU6050(float dt) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true);

  if (Wire.available() >= 14) {
    uint8_t axH = Wire.read(); uint8_t axL = Wire.read();
    uint8_t ayH = Wire.read(); uint8_t ayL = Wire.read();
    uint8_t azH = Wire.read(); uint8_t azL = Wire.read();
    Wire.read(); Wire.read(); // Skip data suhu
    uint8_t gxH = Wire.read(); uint8_t gxL = Wire.read();
    uint8_t gyH = Wire.read(); uint8_t gyL = Wire.read();
    uint8_t gzH = Wire.read(); uint8_t gzL = Wire.read();

    int16_t AcX = (int16_t)(axH << 8 | axL);
    int16_t AcY = (int16_t)(ayH << 8 | ayL);
    int16_t AcZ = (int16_t)(azH << 8 | azL);
    int16_t GyX = (int16_t)(gxH << 8 | gxL);
    int16_t GyY = (int16_t)(gyH << 8 | gyL);
    int16_t GyZ = (int16_t)(gzH << 8 | gzL);

    // Scaling Factor standar (Accel +-2g = 16384 LSB/g, Gyro +-250 deg/s = 131 LSB/deg/s)
    float ax = AcX / 16384.0;
    float ay = AcY / 16384.0;
    float az = AcZ / 16384.0;

    float gx = GyX / 131.0;
    float gy = GyY / 131.0;
    float gz = (GyZ / 131.0) - gz_offset;

    float rollAcc  = atan2(ay, az) * 180.0 / PI;
    float pitchAcc = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0 / PI;

    // Complementary Filter
    roll_6dof  = 0.96 * (roll_6dof + gx * dt) + 0.04 * rollAcc;
    pitch_6dof = 0.96 * (pitch_6dof + gy * dt) + 0.04 * pitchAcc;

    // Integrasi Yaw (Deadband Filter 0.5 deg/s untuk menekan drift residual)
    if (abs(gz) > 0.5) {
      yaw_6dof += gz * dt;
    }
  }
}

// Membaca stream string Razor 9-DOF tanpa blocking delay
void readRazorIMU() {
  while (razorSerial.available() > 0) {
    char c = razorSerial.read();
    if (c == '\n') {
      razorBuffer.trim();
      if (razorBuffer.startsWith("#YPR=")) {
        String data = razorBuffer.substring(5);
        int comma1 = data.indexOf(',');
        int comma2 = data.indexOf(',', comma1 + 1);

        if (comma1 != -1 && comma2 != -1) {
          yaw_9dof   = data.substring(0, comma1).toFloat();
          pitch_9dof = data.substring(comma1 + 1, comma2).toFloat();
          roll_9dof  = data.substring(comma2 + 1).toFloat();
        }
      }
      razorBuffer = "";
    } else if (c != '\r') {
      razorBuffer += c;
    }
  }
}

void setup() {
  Serial.begin(115200);

  Wire.begin();
  initMPU();
  calibrateGyroZ();

  razorSerial.begin(57600);
  prevTime = millis();
}

void loop() {
  // Kosongkan buffer serial Razor sesering mungkin
  readRazorIMU();

  unsigned long currentTime = millis();
  float dt = (currentTime - prevTime) / 1000.0;

  // Eksekusi pembaruan data & plot setiap 50 ms (20 Hz)
  if (dt >= 0.05) {
    prevTime = currentTime;

    readMPU6050(dt);

    // Output Serial Plotter
    Serial.print("Roll_6DOF:");   Serial.print(roll_6dof, 2);   Serial.print(",");
    Serial.print("Roll_9DOF:");   Serial.print(roll_9dof, 2);   Serial.print(",");
    Serial.print("Pitch_6DOF:");  Serial.print(pitch_6dof, 2);  Serial.print(",");
    Serial.print("Pitch_9DOF:");  Serial.print(pitch_9dof, 2);  Serial.print(",");
    Serial.print("Yaw_6DOF:");    Serial.print(yaw_6dof, 2);    Serial.print(",");
    Serial.print("Yaw_9DOF:");    Serial.println(yaw_9dof, 2);
  }
}
