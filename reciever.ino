#include <esp_now.h>
#include <WiFi.h>

// Stepper pins
#define STEP_PIN 25
#define DIR_PIN 26
#define EN_PIN 27

// Limit switch pins
#define LIMIT_UP 32    // Upper limit switch
#define LIMIT_DOWN 33  // Lower limit switch

// Stepper settings
#define STEPS_PER_REV 200
#define MICROSTEPS 1
#define TARGET_RPM 50

// Motion states
enum MotionState {
  STOPPED,
  MOVING_UP,
  MOVING_DOWN,
  LIMIT_HIT
};

// Data structure for receiving
typedef struct struct_message {
  int direction; // 1 = up, -1 = down, 0 = stop
} struct_message;

struct_message incomingData;
MotionState currentState = STOPPED;
unsigned long halfDelay;
bool limitUpHit = false;
bool limitDownHit = false;

// Callback when data is received (updated for ESP32 v3.x)
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingDataBytes, int len) {
  memcpy(&incomingData, incomingDataBytes, sizeof(incomingData));
  
  Serial.println("\n--- Data Received ---");
  Serial.print("From MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", info->src_addr[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
  Serial.print("Direction: ");
  Serial.print(incomingData.direction);
  Serial.print(" | ");
  if (incomingData.direction == 1) Serial.println("UP");
  else if (incomingData.direction == -1) Serial.println("DOWN");
  else Serial.println("STOP");
  
  // Check limits before allowing movement
  if (incomingData.direction == 1 && !limitUpHit) {
    // Move up
    Serial.println("→ Moving UP");
    digitalWrite(DIR_PIN, HIGH);
    digitalWrite(EN_PIN, LOW);  // Enable motor (active LOW for your TB6600)
    currentState = MOVING_UP;
  } 
  else if (incomingData.direction == -1 && !limitDownHit) {
    // Move down
    Serial.println("→ Moving DOWN");
    digitalWrite(DIR_PIN, LOW);
    digitalWrite(EN_PIN, LOW);  // Enable motor (active LOW for your TB6600)
    currentState = MOVING_DOWN;
  } 
  else if (incomingData.direction == 1 && limitUpHit) {
    Serial.println("⚠ Cannot move UP - Upper limit reached!");
    digitalWrite(EN_PIN, LOW);  // Keep enabled but don't step
    currentState = STOPPED;
  }
  else if (incomingData.direction == -1 && limitDownHit) {
    Serial.println("⚠ Cannot move DOWN - Lower limit reached!");
    digitalWrite(EN_PIN, LOW);  // Keep enabled but don't step
    currentState = STOPPED;
  }
  else {
    // Stop
    Serial.println("→ STOPPED");
    digitalWrite(EN_PIN, LOW);  // Keep enabled (holds position)
    currentState = STOPPED;
  }
  Serial.println("--------------------");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n========================================");
  Serial.println("    ESP-NOW STEPPER RECEIVER v1.0");
  Serial.println("========================================\n");
  
  // Setup stepper pins
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, LOW); // TB6600: LOW = Enabled (for your board)
  Serial.println("✓ Stepper pins configured");
  
  // Setup limit switches (internal pullup)
  pinMode(LIMIT_UP, INPUT_PULLUP);
  pinMode(LIMIT_DOWN, INPUT_PULLUP);
  Serial.println("✓ Limit switches configured");
  
  // Calculate step timing
  halfDelay = (unsigned long)((60000000.0 / (STEPS_PER_REV * MICROSTEPS * TARGET_RPM)) / 2.0);
  Serial.print("✓ Step timing: ");
  Serial.print(halfDelay);
  Serial.println(" µs half-delay");
  Serial.print("  Target speed: ");
  Serial.print(TARGET_RPM);
  Serial.println(" RPM\n");
  
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  
  // Print MAC address
  Serial.println("📡 Network Configuration:");
  Serial.print("   MAC Address: ");
  Serial.println(WiFi.macAddress());
  
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ Error initializing ESP-NOW");
    return;
  }
  Serial.println("✓ ESP-NOW initialized");
  
  // Register receive callback
  esp_now_register_recv_cb(OnDataRecv);
  Serial.println("✓ Receive callback registered");
  
  Serial.println("\n========================================");
  Serial.println("   RECEIVER READY - Waiting for data");
  Serial.println("========================================\n");
}

void loop() {
  // Check limit switches
  limitUpHit = (digitalRead(LIMIT_UP) == LOW);    // Active low
  limitDownHit = (digitalRead(LIMIT_DOWN) == LOW); // Active low
  
  // Stop if limit hit
  if ((limitUpHit && currentState == MOVING_UP) || 
      (limitDownHit && currentState == MOVING_DOWN)) {
    digitalWrite(EN_PIN, LOW);  // Keep enabled (holds position)
    currentState = LIMIT_HIT;
    Serial.println("⚠️ Limit switch hit! Motor stopped but holding position.");
  }
  
  // Step the motor if moving
  if (currentState == MOVING_UP || currentState == MOVING_DOWN) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(halfDelay);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(halfDelay);
    
    // Debug: Print every 1000 steps
    static unsigned long stepCount = 0;
    stepCount++;
    if (stepCount % 1000 == 0) {
      Serial.print("⚙️ Steps: ");
      Serial.print(stepCount);
      Serial.print(" | EN_PIN: ");
      Serial.print(digitalRead(EN_PIN));
      Serial.print(" | DIR_PIN: ");
      Serial.print(digitalRead(DIR_PIN));
      Serial.print(" | State: ");
      Serial.println(currentState == MOVING_UP ? "UP" : "DOWN");
    }
  }
}
