#include <Arduino.h>
#include <IO7F8266.h>

String user_html = "<p><input type='text' name='meta.yourVar' placeholder='Your Custom Config'>";
int a = 1;
char *ssid_pfix = (char *)"SKH_Rotary";
unsigned long lastPublishMillis = -pubInterval;
volatile long lastRotatePub = 0;
const int pulseA = 12;
const int pulseB = 13;
const int pushSW = 2;
volatile int lastEncoded = 0;
volatile long encoderValue = 0;
int prev_value = 0;
volatile bool pressed = false;
volatile bool rotated = false;
int customVar1;
void handleUserMeta()
{
  // USER CODE EXAMPLE : meta data update
  // If any meta data updated on the Internet, it can be stored to local variable to use for the logic
  // in cfg["meta"]["XXXXX"], XXXXX should match to one in the user_html
  customVar1 = cfg["meta"]["yourVar"];
  // USER CODE EXAMPLE
}
IRAM_ATTR void handleRotary()
{
  int MSB = digitalRead(pulseA);
  int LSB = digitalRead(pulseB);

  int encoded = (MSB << 1) | LSB;
  int sum = (lastEncoded << 2) | encoded;
  if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011)
    encoderValue++;
  if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000)
    encoderValue--;
  lastEncoded = encoded;
  if (encoderValue > 255)
  {
    encoderValue = 255;
  }
  else if (encoderValue < 0)
  {
    encoderValue = 0;
  }
  if (prev_value != encoderValue)
  {
    rotated = true;
  }
}

IRAM_ATTR void pushed()
{
  pressed = true;
}

void publishData()
{
  StaticJsonDocument<512> root;
  JsonObject data = root.createNestedObject("d");

  if (rotated)
  {
    if (prev_value < encoderValue)
    {
      data["switch"] = "on";
    }
    else
    {
      data["switch"] = "off";
    }
    prev_value = encoderValue;
    lastRotatePub = millis();
    rotated = false;
  }
  if (pressed)
  {
    data["pressed"] = "true";
    delay(200);
    pressed = false;
  }
  else
  {
    data["pressed"] = "false";
  }

  serializeJson(root, msgBuffer);
  client.publish(evtTopic, msgBuffer);
}

void setup()
{
  Serial.begin(115200);
  pinMode(pushSW, INPUT_PULLUP);
  pinMode(pulseA, INPUT_PULLUP);
  pinMode(pulseB, INPUT_PULLUP);
  attachInterrupt(pushSW, pushed, FALLING);
  attachInterrupt(pulseA, handleRotary, CHANGE);
  attachInterrupt(pulseB, handleRotary, CHANGE);

  initDevice();
  JsonObject meta = cfg["meta"];
  pubInterval = meta.containsKey("pubInterval") ? meta["pubInterval"] : 0;
  lastPublishMillis = -pubInterval;

  WiFi.mode(WIFI_STA);
  WiFi.begin((const char *)cfg["ssid"], (const char *)cfg["w_pw"]);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  // main setup
  Serial.printf("\nIP address : ");
  Serial.println(WiFi.localIP());

  set_iot_server();
  iot_connect();
}

void loop()
{
  if (!client.connected())
  {
    iot_connect();
  }
  client.loop();
  if (abs(prev_value - encoderValue) < 8 || rotated && (millis() - lastRotatePub < 500))
  {
    rotated = false;
  }

  if (pressed || rotated || ((pubInterval != 0) && (millis() - lastPublishMillis > pubInterval)))
  {
    publishData();
    lastPublishMillis = millis();
  }
}