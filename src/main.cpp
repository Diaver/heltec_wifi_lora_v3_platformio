#include "Arduino.h"
#include "SPI.h"
#include <U8g2lib.h>
#include <RadioLib.h>
#include "images.h"

#include <Wire.h>
#include "WiFi.h"


// SX1262 has the following connections:
#define LORA_NSS_PIN     8
#define LORA_RESET_PIN  12
#define LORA_DIO1_PIN   14
#define LORA_BUSY_PIN   13

#define OLED_RESET 21 // Reset pin # (or -1 if sharing Arduino reset pin)
#define OLED_SDA 17
#define OLED_SCL 18

U8G2_SSD1306_128X64_NONAME_F_SW_I2C display(U8G2_R0, /* clock=*/ OLED_SCL, /* data=*/ OLED_SDA, /* reset=*/ OLED_RESET);   // All Boards without Reset of the Display


#define RADIO_DIO_1    14
#define RADIO_NSS      8
#define RADIO_RESET    12
#define RADIO_BUSY     13

#define LORA_CLK       9
#define LORA_MISO      11
#define LORA_MOSI      10

SX1262 Lora = new Module(LORA_NSS_PIN, LORA_DIO1_PIN, LORA_RESET_PIN, LORA_BUSY_PIN);


void logo(){
	display.clearBuffer();
	display.drawXBM(0,5, logo_width, logo_height,(const unsigned char *)logo_bits);
	display.sendBuffer();
}

void WIFISetUp(void)
{
	// Set WiFi to station mode and disconnect from an AP if it was previously connected
	WiFi.disconnect(true);
	delay(100);
	WiFi.mode(WIFI_STA);
	WiFi.setAutoConnect(true);
	WiFi.begin("Your WiFi SSID","Your Password");//fill in "Your WiFi SSID","Your Password"
	delay(100);


	byte count = 0;
	while(WiFi.status() != WL_CONNECTED && count < 10)
	{
		count ++;
		delay(500);
		Serial.println("Connecting...");

		display.clearBuffer();
		display.drawStr(0, 10, "Connecting...");
		display.sendBuffer();
	}

	display.clearBuffer();
	if(WiFi.status() == WL_CONNECTED)
	{
		Serial.println("Connected.");

		display.clearBuffer();
		display.drawStr(0, 10, "Connected.");
		display.sendBuffer();
//		delay(500);
	}
	else
	{
		Serial.println("Failed to connect.");
		display.clearBuffer();
		display.drawStr(0, 10, "Failed to connect.");
		display.sendBuffer();
		//while(1);
	}
	display.drawStr(0, 20, "WiFi setup done");
	display.sendBuffer();
	delay(500);
}

void WIFIScan(unsigned int value)
{
	unsigned int i;
    WiFi.mode(WIFI_STA);

	for(i=0;i<value;i++)
	{
		display.drawStr(0, 20, "Starting scan.");
		display.sendBuffer();

		int n = WiFi.scanNetworks();
		display.drawStr(0, 30, "Scan done");
		display.sendBuffer();
		delay(500);
		display.clearBuffer();

		if (n == 0)
		{
			display.clearBuffer();
			display.drawStr(0, 0, "No network found");
			display.sendBuffer();
			//while(1);
		}
		else
		{
			display.drawStr(0, 0, String(n).c_str());
			display.drawStr(14, 0, "Networks found:");
			display.sendBuffer();
			delay(500);

			for (int i = 0; i < n; ++i) {
			// Print SSID and RSSI for each network found
				display.drawStr(0, (i+1)*9, String(i + 1).c_str());
				display.drawStr(6, (i+1)*9, ":");
				display.drawStr(12,(i+1)*9, String(WiFi.SSID(i)).c_str());
				display.drawStr(90,(i+1)*9, " (");
				display.drawStr(98,(i+1)*9, String(WiFi.RSSI(i)).c_str());
				display.drawStr(114,(i+1)*9, ")");
				delay(10);
			}
		}

		display.sendBuffer();
		delay(800);
		display.clearBuffer();
	}
}


bool resendflag=false;
bool deepsleepflag=false;

int transmissionState = RADIOLIB_ERR_NONE;

// flag to indicate transmission or reception state
bool transmitFlag = false;

unsigned int counter = 0;

// flag to indicate that a packet was sent or received
volatile bool operationDone = false;

// this function is called when a complete packet
// is transmitted or received by the module
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
void setFlag(void) {
  // we sent or received a packet, set the flag
  operationDone = true;
}

void setup(void) {

	Serial.begin(115200);
	pinMode(9, OUTPUT);
	digitalWrite(9, 0);	// default output in I2C mode for the SSD1306 test shield: set the i2c adr to 0

  	display.begin();
	display.setFont(u8g2_font_5x8_mf);

	logo();
	delay(3000);
	display.clear();

	WIFISetUp();
	WiFi.disconnect(); //Reinitialize WiFi
	WiFi.mode(WIFI_STA);
	delay(100);

	WIFIScan(1);

	uint64_t chipid=ESP.getEfuseMac();//The chip ID is essentially its MAC address(length: 6 bytes).
	Serial.printf("ESP32ChipID=%04X",(uint16_t)(chipid>>32));//print High 2 bytes
	Serial.printf("%08X\n",(uint32_t)chipid);//print Low 4bytes.


	SPI.begin(LORA_CLK,LORA_MISO,LORA_MOSI,RADIO_NSS);

	int state = Lora.begin();

	if (state == RADIOLIB_ERR_NONE) {
		Serial.println(F("Lora Initialized!"));

		display.clearBuffer();
		display.drawStr(0, 10, "Lora Initialized!");
		display.sendBuffer();
	} else {
		Serial.print(F("Lora failed, code "));
		Serial.println(state);

		display.clearBuffer();
		display.drawStr(0, 10, "Lora failed, code:" + state);
		display.sendBuffer();

		while (true);
	}

	if (Lora.setFrequency(915.0) == RADIOLIB_ERR_INVALID_FREQUENCY) {
		Serial.println(F("Selected value is invalid for FREQUENCY"));
		while (true);
 	}

	if (Lora.setSpreadingFactor(7) == RADIOLIB_ERR_INVALID_SPREADING_FACTOR) {
		Serial.println(F("Selected value is invalid for SPREADING_FACTOR!"));
		while (true);
	}

	if (Lora.setCodingRate(5) == RADIOLIB_ERR_INVALID_CODING_RATE) {
		Serial.println(F("Selected value is invalid for CODING_RATE!"));
		while (true);
	}

	 // set the function that will be called
  	// when new packet is received
 	 Lora.setDio1Action(setFlag);

   /* // send the first packet on this node
    Serial.print(F("[SX1262] Sending first packet ... "));

	
	display.clearBuffer();
	display.drawStr(0, 10, "Sending first packet ...");
	display.sendBuffer();

    transmissionState = Lora.startTransmit("Hello" + String(counter++));
    transmitFlag = true;*/

    // start listening for LoRa packets on this node
    Serial.print(F("[SX1262] Starting to listen ... "));

	display.clearBuffer();
	display.drawStr(0, 10, "Starting to listen...");
	display.sendBuffer();

    state = Lora.startReceive();
    if (state == RADIOLIB_ERR_NONE) {
      	Serial.println(F("success!"));

		
		display.drawStr(0, 20, "success!");
		display.sendBuffer();

    } else {
		Serial.print(F("failed, code "));
		Serial.println(state);

		display.drawStr(0, 20, "failed, code " + state);
		display.sendBuffer();

      while (true);
    }
}



void loop() {
  // check if the previous operation finished
  if(operationDone) {
    // reset flag
    operationDone = false;

    if(transmitFlag) {
      // the previous operation was transmission, listen for response
      // print the result
      if (transmissionState == RADIOLIB_ERR_NONE) {
        // packet was successfully sent
        Serial.println(F("transmission finished!"));

      } else {
        Serial.print(F("failed, code "));
        Serial.println(transmissionState);

      }

      // listen for response
      Lora.startReceive();
      transmitFlag = false;

    } else {
      // the previous operation was reception
      // print data and send another packet
      String str;
      int state = Lora.readData(str);

      if (state == RADIOLIB_ERR_NONE) {


        // packet was successfully received
        Serial.println(F("[SX1262] Received packet!"));

        // print data of the packet
        Serial.print(F("[SX1262] Data:\t\t"));
        Serial.println(str);

		int16_t RssiDetection= abs(Lora.getRSSI());
		if(RssiDetection < 65)
		{
		digitalWrite(25, HIGH);  
		}
		else
		{
		digitalWrite(25, LOW);  
		}        


		String getRSSI = String(RssiDetection);


        // print RSSI (Received Signal Strength Indicator)
        Serial.print(F("[SX1262] RSSI:\t\t"));
        Serial.print(getRSSI);
        Serial.println(F(" dBm"));

        // print SNR (Signal-to-Noise Ratio)
        Serial.print(F("[SX1262] SNR:\t\t"));
        Serial.print(Lora.getSNR());
        Serial.println(F(" dB"));


		display.clearBuffer();
		display.drawStr(0, 50, String("Packet " + (String)(counter-1) + " sent done").c_str());
		//display.drawStr(0, 0, String("Received Size  " + str.length() + " packages:").c_str());
		display.drawStr(0, 10, str.c_str());
		display.drawStr(0, 20, String("With " + getRSSI + "db").c_str());
		display.sendBuffer();
		delay(100);

      }

      // wait a second before transmitting again
      delay(1000);

      // send another one
      Serial.print(F("[SX1262] Sending another packet ... "));

      transmissionState = Lora.startTransmit("Hello" + String(counter++));
      transmitFlag = true;
    }
  
  }
}