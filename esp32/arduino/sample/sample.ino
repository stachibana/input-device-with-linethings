#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Device Name: Maximum 30 bytes
#define DEVICE_NAME "LINE Things Trial ESP32"

// User service UUID: Change this to your generated service UUID
#define USER_SERVICE_UUID "4875aa75-6b69-4faa-a59e-46f2f0f03757"
// User service characteristics
#define WRITE_CHARACTERISTIC_UUID "E9062E71-9E62-4BC6-B0D3-35CDCD9B027B"
#define NOTIFY_CHARACTERISTIC_UUID "62FBD229-6EDD-4D1A-B554-5C4E1BB29169"

// PSDI Service UUID: Fixed value for Developer Trial
#define PSDI_SERVICE_UUID "E625601E-9E55-4597-A598-76018A0D293D"
#define PSDI_CHARACTERISTIC_UUID "26E2B12B-85F0-4F3F-9FDD-91D114270E6E"

#define BUTTON 0
#define LED1 2
// Added 3 switches
#define SWITCH1 4
#define SWITCH2 5
#define SWITCH3 21


BLEServer* thingsServer;
BLESecurity *thingsSecurity;
BLEService* userService;
BLEService* psdiService;
BLECharacteristic* psdiCharacteristic;
BLECharacteristic* writeCharacteristic;
BLECharacteristic* notifyCharacteristic;

bool deviceConnected = false;
bool oldDeviceConnected = false;

volatile int btnAction = 0;
// Added
volatile int swc1Action = 0;
volatile int swc2Action = 0;
volatile int swc3Action = 0;

class serverCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
   deviceConnected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

class writeCallback: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *bleWriteCharacteristic) {
    std::string value = bleWriteCharacteristic->getValue();
    if ((char)value[0] <= 1) {
      digitalWrite(LED1, (char)value[0]);
    }
  }
};

void setup() {
  Serial.begin(115200);
  
  pinMode(LED1, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);
  attachInterrupt(BUTTON, buttonAction, CHANGE);

  // Added
  pinMode(SWITCH1, INPUT);
  attachInterrupt(SWITCH1, switch1Action, CHANGE);
  pinMode(SWITCH2, INPUT);
  attachInterrupt(SWITCH2, switch2Action, CHANGE);
  pinMode(SWITCH3, INPUT);
  attachInterrupt(SWITCH3, switch3Action, CHANGE);
  
  BLEDevice::init("");
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_NO_MITM);

  // Security Settings
  BLESecurity *thingsSecurity = new BLESecurity();
  thingsSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
  thingsSecurity->setCapability(ESP_IO_CAP_NONE);
  thingsSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  setupServices();
  startAdvertising();
  Serial.println("Ready to Connect");
}

void loop() {
  uint8_t btnValue;
  
  while (btnAction > 0 && deviceConnected) {
    btnValue = !digitalRead(BUTTON);
    Serial.println(btnValue);
    
    btnAction = 0;
    notifyCharacteristic->setValue(&btnValue, 1);
    notifyCharacteristic->notify();
    delay(20);
  }

  // Added
  uint8_t switch1Value;
  while (swc1Action > 0 && deviceConnected) {
    switch1Value = digitalRead(SWITCH1);
    Serial.println(switch1Value);
    
    swc1Action = 0;
    notifyCharacteristic->setValue(&switch1Value, 2);
    notifyCharacteristic->notify();
    delay(20);
  }

  uint8_t switch2Value;
  while (swc2Action > 0 && deviceConnected) {
    switch2Value = digitalRead(SWITCH2);
    Serial.println(switch2Value);
    
    swc2Action = 0;
    notifyCharacteristic->setValue(&switch2Value, 3);
    notifyCharacteristic->notify();
    delay(20);
  }
  
  uint8_t switch3Value;
  while (swc3Action > 0 && deviceConnected) {
    switch3Value = digitalRead(SWITCH3);
    Serial.println(switch3Value);
    
    swc3Action = 0;
    notifyCharacteristic->setValue(&switch3Value, 4);
    notifyCharacteristic->notify();
    delay(20);
  }
  
  // Disconnection
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // Wait for BLE Stack to be ready
    thingsServer->startAdvertising(); // Restart advertising
    oldDeviceConnected = deviceConnected;
  }
  // Connection
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
}

void setupServices(void) {
  // Create BLE Server
  thingsServer = BLEDevice::createServer();
  thingsServer->setCallbacks(new serverCallbacks());

  // Setup User Service
  userService = thingsServer->createService(USER_SERVICE_UUID);
  // Create Characteristics for User Service
  writeCharacteristic = userService->createCharacteristic(WRITE_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);
  writeCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  writeCharacteristic->setCallbacks(new writeCallback());

  notifyCharacteristic = userService->createCharacteristic(NOTIFY_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  notifyCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  BLE2902* ble9202 = new BLE2902();
  ble9202->setNotifications(true);
  ble9202->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  notifyCharacteristic->addDescriptor(ble9202);

  // Setup PSDI Service
  psdiService = thingsServer->createService(PSDI_SERVICE_UUID);
  psdiCharacteristic = psdiService->createCharacteristic(PSDI_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ);
  psdiCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);

  // Set PSDI (Product Specific Device ID) value
  uint64_t macAddress = ESP.getEfuseMac();
  psdiCharacteristic->setValue((uint8_t*) &macAddress, sizeof(macAddress));

  // Start BLE Services
  userService->start();
  psdiService->start();
}

void startAdvertising(void) {
  // Start Advertising
  BLEAdvertisementData scanResponseData = BLEAdvertisementData();
  scanResponseData.setFlags(0x06); // GENERAL_DISC_MODE 0x02 | BR_EDR_NOT_SUPPORTED 0x04
  scanResponseData.setName(DEVICE_NAME);

  thingsServer->getAdvertising()->addServiceUUID(userService->getUUID());
  thingsServer->getAdvertising()->setScanResponseData(scanResponseData);
  thingsServer->getAdvertising()->start();
}

void buttonAction() {
  btnAction++;
}

// Added
void switch1Action() {
  swc1Action++;
}

void switch2Action() {
  swc2Action++;
}

void switch3Action() {
  swc3Action++;
}
