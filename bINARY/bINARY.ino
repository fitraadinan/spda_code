#include <SPI.h>
#include <MFRC522.h>
#include <FirebaseESP8266.h> // Include the Firebase library
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

struct Node {
  byte UID[7];
  Node* left;
  Node* right;
};

HTTPClient http;

#define SS_PIN D2
#define RST_PIN D1

MFRC522 rfid(SS_PIN, RST_PIN); // Create rfid instance

// Initialize Firebase credentials and database instance
#define FIREBASE_HOST "spdacoba-default-rtdb.firebaseio.com" 
#define FIREBASE_AUTH "Z34k0l7JTX9xwUFF9x4CroYz4XLjnWbUrPwljbP4"

String url = "http://spda.17management.my.id/api/documents/update-document";
FirebaseData firebaseData;
int device_id = 4;
String checkUrl = "http://spda.17management.my.id/api/devices/start-check-status";
int n = 1;
String tags[10];
int numTags = 0;
int a = 0;
unsigned long lastMillis = 0;
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


Node* root = NULL;

Node* newNode(byte UID[]) {
  Node* node = new Node;
  memcpy(node->UID, UID, 7);
  node->left = NULL;
  node->right = NULL;
  return node;
}

Node* insert(Node* node, byte UID[]) {
  if (node == NULL) {
    return newNode(UID);
  }
  if (memcmp(node->UID, UID, 7) < 0) {
    node->right = insert(node->right, UID);
  } else {
    node->left = insert(node->left, UID);
  }
  return node;
}

bool search(Node* node, byte UID[]) {
  if (node == NULL) {
    return false;
  }
  int cmp = memcmp(node->UID, UID, 7);
  if (cmp == 0) {
    return true;
  }
  if (cmp < 0) {
    return search(node->right, UID);
  } else {
    return search(node->left, UID);
  }
}

void printTree(Node* node) {
  if (node == NULL) {
    return;
  }
  printTree(node->left);
  Serial.print("Card UID: ");
  for (byte i = 0; i < 7; i++) {
    Serial.print(node->UID[i] < 0x10 ? "" : "");
    Serial.print(node->UID[i], HEX);
  }
  Serial.println();
  printTree(node->right);
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
  if (millis() - lastMillis >= 1*2*1000UL) 
  {
    Serial.println("Start Milis");
    //get ready for the next iteration
    lastMillis = millis();  
    detect();
    printTree(root);
  }

  if (millis() - lastCheck >= 1*30*1000UL)
  {
    lastCheck = millis();
    checkConnect();
  }
}

void detect()
{
  // Look for new cards
  if ( ! rfid.PICC_IsNewCardPresent()) {
    notFoundCard();
  }

  // Select one of the cards
  if ( ! rfid.PICC_ReadCardSerial()) {
    return;
  }

  Serial.print("Num Tag: ");
    Serial.println(numTags);
  
  // Print the UID value of the card
  String new_tag = printUID();
  Serial.println(new_tag);

  // Insert or search for the card in the binary tree
  insertOrSearch();

  // Check if the tag is already in the array
  bool checkTag = checkTagInArray(new_tag);
  
  // Add the tag to the array if it doesn't already exist
  addTagIfNotAlready(new_tag, checkTag);

  String path = "/Data/";
  getTagFromDB(path, new_tag);

  // Save tag UID into Firebase with device ID
  setValue(path, new_tag);

  // Print the contents of the binary tree
  Serial.println("Contents of the binary tree:");
  printTree(root);
  Serial.println();
  
  delay(1000);
}

void startConnect()
{
  Firebase.setString(firebaseData, "Node/Node 3", "Connected");
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

    if(currentDeviceId != device_id)
    {
      updateData(tag);
    }
  }
}

void insertOrSearch()
{
  byte UID[7];
  memcpy(UID, rfid.uid.uidByte, 7);
  if (root == NULL) {
    root = insert(root, UID);
    Serial.println("Card added to the binary tree.");
  } else if (search(root, UID)) {
    Serial.println("Card already exists in the binary tree.");
  } else {
    insert(root, UID);
    Serial.println("Card added to the binary tree.");
  }
}

void setValue(String path, String tag)
{
  Firebase.setInt(firebaseData, path + tag + "/device_id", device_id);
  if (firebaseData.httpCode() == HTTP_CODE_OK) {
    Serial.println("Firebase set response: " + String(firebaseData.intData()));
  }
  else {
    Serial.println("Firebase push failed.");
    Serial.println(firebaseData.errorReason());
  }
}

void updateData(String temp)
{
    Serial.println(device_id);
    http.begin(url);      //Specify request destination
    http.addHeader("Content-Type", "application/json");  //Specify content-type header
 
    int httpCode = http.POST("{\"device_id\":\""+String (device_id)+"\",\"uuid\":\""+temp+"\"}");   //Send the request
    String payload = http.getString();                  //Get the response payload
 
    Serial.println(httpCode);   //Print HTTP return code
    Serial.println(payload);    //Print request response payload
 
    http.end();
}

void deleteData(String temp)
{
    String zero = "0";
    http.begin(url);      //Specify request destination
    http.addHeader("Content-Type", "application/json");  //Specify content-type header
 
    int httpCode = http.POST("{\"device_id\":\""+zero+"\",\"uuid\":\""+temp+"\"}");   //Send the request
    String payload = http.getString();                  //Get the response payload
 
    Serial.println(httpCode);   //Print HTTP return code
    Serial.println(payload);    //Print request response payload
 
    http.end();
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
      Serial.println("Card Detected");
      return;
    }

    // Use the tag to set the device_id to 0 in Firebase Realtime Database
    String path = "/Data/";
    Firebase.setInt(firebaseData, path + tag + "/device_id", 0);
    check(tag);
    deleteData(tag);
    Serial.println("Delete UID");
    deleteUID(tag);
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

void check(String tag)
{
  byte UID[7];
  for (byte i = 0; i < 7; i++) {
    UID[i] = strtoul(tag.substring(i * 2, i * 2 + 2).c_str(), NULL, 16);
  }

  if(search(root, UID))
  {
    Serial.println("Found!");
    String tag = "";
    // Get Tag from detect
    for (byte i = 0; i < 7; i++) {
      tag += (UID[i] < 0x10 ? " " : "");
      tag += String(UID[i], HEX);
    }    
    Serial.print("This Tag is : ");
    Serial.println(tag);
  } else {
    Serial.println("Not Found!");
  }
}


void deleteUID(String tag) {
  byte UID[7];
  // Convert the UID string to an array of bytes
  for (byte i = 0; i < 7; i++) {
    UID[i] = strtoul(tag.substring(i * 2, i * 2 + 2).c_str(), NULL, 16);
  }

  Node* node = root;
  Node* parent = NULL;

  // Search for the node to delete
  while (node != NULL) {
    int cmp = memcmp(node->UID, UID, 7);
    if (cmp == 0) {
      break;
    }
    parent = node;
    node = cmp < 0 ? node->right : node->left;
  }

  // If the node was found, delete it
  if (node != NULL) {
    if (node->left == NULL) {
      if (parent == NULL) {
        root = node->right;
      } else if (node == parent->left) {
        parent->left = node->right;
      } else {
        parent->right = node->right;
      }
      delete node;
      Serial.println("Card deleted from the binary tree.");
    } else if (node->right == NULL) {
      if (parent == NULL) {
        root = node->left;
      } else if (node == parent->left) {
        parent->left = node->left;
      } else {
        parent->right = node->left;
      }
      delete node;
      Serial.println("Card deleted from the binary tree.");
    } else {
      Node* next = node->right;
      parent = node;
      while (next->left != NULL) {
        parent = next;
        next = next->left;
      }
      memcpy(node->UID, next->UID, 7);
      if (next == parent->left) {
        parent->left = next->right;
      } else {
        parent->right = next->right;
      }
      delete next;
      Serial.println("Card deleted from the binary tree.");
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
