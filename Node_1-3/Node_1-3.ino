#include <SPI.h>
#include <MFRC522.h>
#include <FirebaseESP8266.h> // Include the Firebase library
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

HTTPClient http;

#define SS_PIN D2
#define RST_PIN D1

MFRC522 rfid(SS_PIN, RST_PIN); // Create rfid instance

// Initialize Firebase credentials and database instance
#define FIREBASE_HOST "spdacoba-default-rtdb.firebaseio.com" 
#define FIREBASE_AUTH "Z34k0l7JTX9xwUFF9x4CroYz4XLjnWbUrPwljbP4"

String url = "http://spda.17management.my.id/api/update-document";
FirebaseData firebaseData;
int device_id = 1;
String checkUrl = "http://spda.17management.my.id/api/devices/start-check-status";
int n = 1;
String tags[10];
int numTags = 0;
int a = 0;
unsigned long lastCheck = 0;

const char ssid[] = "ssid";
const char pass[] = "Dina090?";

void connect()
{
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to Wi-Fi...");
  }
  Serial.println("Connected to Wi-Fi.");
}

void setup() {
  Serial.begin(115200); // Initialize serial communication
  SPI.begin(); // Initialize SPI communication
  rfid.PCD_Init(); // Initialize RFID reader

    // Connect to Wi-Fi
  connect();

  // Initialize Firebase
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  if(WiFi.status() == WL_CONNECTED)
  {
    startConnect();
  }
}

void loop() {

    if (millis() - lastCheck >= 1*30*1000UL)
    {
      lastCheck = millis();
      checkConnect();
    }
    
    // Look for new cards
    if (!rfid.PICC_IsNewCardPresent()) {
      notFoundCard();
    }
  
    // Select one of the cards
    if (!rfid.PICC_ReadCardSerial()) {
      return;
    }
    Serial.print("Num Tag: ");
    Serial.println(numTags);
    Serial.print("Ini ulangan yang ke-");
    Serial.println (n);
    n++;
  
    // Print the tag UID
    String new_tag = printUID();

    // Check if the tag is already in the array
    bool checkTag = checkTagInArray(new_tag);

    // Add the tag to the array if it doesn't already exist
    addTagIfNotAlready(new_tag, checkTag);

    Serial.println(new_tag);

    String path = "/Daftar/";
    getTagFromDB(path, new_tag);

    // Save tag UID into Firebase with device ID
//    String path = "/Daftar/";
    setValue(path, new_tag);

    delay(2000);
}

void startConnect()
{
  Firebase.setString(firebaseData, "Node/Node Register", "Connected");
}

String printUID()
{
  Serial.print("Tag UID: ");
    String tag = "";
    // Get Tag from detect
    for (byte i = 0; i < rfid.uid.size; i++) {
      tag += (rfid.uid.uidByte[i] < 0x10 ? " " : "");
      tag += String(rfid.uid.uidByte[i], HEX);
    }

    return tag;
}

bool checkTagInArray(String tag)
{
  bool tagExists = false;
    for (int i = 0; i < numTags; i++) {
      if (tag == tags[i]) {
        tagExists = true;
        break;
      }
    }

    return tagExists;
}

void addTagIfNotAlready(String tag, bool tagExists)
{
  if (!tagExists) {
      if (numTags < 10) {
        tags[numTags++] = tag;
      }
    }
}

void getTagFromDB(String path, String tag)
{
  Firebase.getInt(firebaseData, path + tag + "/device_id");
  if (firebaseData.dataType() == "int") {
    int currentDeviceId = firebaseData.intData();
    String currentTag = tag;
  }
}

void setValue(String path, String tag)
{
  Firebase.setInt(firebaseData, path + "device_id", device_id);
  Firebase.setString(firebaseData, path + "uuid", tag);
  Firebase.setString(firebaseData, path + "status", "Terdeteksi");
  if (firebaseData.httpCode() == HTTP_CODE_OK) {
    Serial.println("Firebase set response: " + String(firebaseData.intData()));
  }
  else {
    Serial.println("Firebase push failed.");
    Serial.println(firebaseData.errorReason());
  }
}

bool detectedCard(String tag)
{
  if(rfid.PICC_IsNewCardPresent())
  {
    if(rfid.PICC_ReadCardSerial())
    {
      String new_tag = printUID();
      if(new_tag == tag)
      {
        Serial.println("*Cocok*");
        if(numTags > 1)
        {
          a++;
        }
        return true;
      } else {
        Serial.println("*Tidak Cocok*");
        return false;
      }
    }
  } else {
    Serial.println("Card Not Detected!");
    return false;
  }
}

void notFoundCard()
{
  if (numTags > 0) {
    // Get the first tag from the array
    String tag = "";
    if(a >= 0)
    {
      tag = tags[a];
    } else {
      tag = tags[0];
    }
    
    bool isDetected = detectedCard(tag);
    if(isDetected)
    {
      return;
    }

    // Use the tag to set the device_id to 0 in Firebase Realtime Database
    String path = "/Daftar/";
    Firebase.setInt(firebaseData, path + "device_id", 0);
    Firebase.setString(firebaseData, path + "uuid", 0);
    Firebase.setString(firebaseData, path + "status", "Tidak Terdeteksi");
    if (firebaseData.httpCode() == HTTP_CODE_OK) {
      Serial.println("Firebase nf response: " + String(firebaseData.intData()));
          // Remove the tag from the array
      for (int i = 0; i < numTags; i++) {
        if (tags[i] == tag) {
          for (int j = i; j < numTags - 1; j++) {
            tags[j] = tags[j+1];
          }
          numTags--;
          a--;
          break;
        }
      }
    }
    else {
      Serial.println("Firebase push failed.");
      Serial.println(firebaseData.errorReason());
    }
  }
}

void checkConnect()
{
    Serial.println("Signal Connection");
    http.begin(checkUrl);      //Specify request destination
    http.addHeader("Content-Type", "application/json");  //Specify content-type header
 
    int httpCode = http.POST("{\"id\":\""+(String) device_id+"\"}");     //Send the request
    String payload = http.getString();                  //Get the response payload
 
    Serial.println(httpCode);   //Print HTTP return code
    Serial.println(payload);    //Print request response payload
 
    http.end(); 
}
