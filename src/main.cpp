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

#define OLED_RESET 21 
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


int transmissionState = RADIOLIB_ERR_NONE;

// flag to indicate transmission or reception state
bool transmitFlag = false;

unsigned int counter = 0;
int LED = 35;

// flag to indicate that a packet was sent or received
volatile bool operationDone = false;



void logo()
{
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
	}
	else
	{
		Serial.println("Failed to connect.");
		display.clearBuffer();
		display.drawStr(0, 10, "Failed to connect.");
		display.sendBuffer();
	}
	display.drawStr(0, 20, "WiFi setup done");
	display.sendBuffer();
	delay(500);
}

void WIFIScan(unsigned int value)
{
    WiFi.mode(WIFI_STA);

	for(int i=0; i<value; i++)
	{
		display.clearBuffer();
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
		}
		else
		{
			display.drawStr(0, 0, String(n).c_str());
			display.drawStr(14, 0, "Networks found:");
			display.sendBuffer();
			delay(500);

			for (int i = 0; i < n; ++i) 
			{
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


// this function is called when a complete packet
// is transmitted or received by the module
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
void setFlag(void) 
{
	operationDone = true;
	Serial.println("operationDone = true");
}

void setup(void) 
{

	Serial.begin(115200);

  	display.begin();
	display.setFont(u8g2_font_NokiaSmallPlain_te );
	
	pinMode(LED, OUTPUT);

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

	if (state == RADIOLIB_ERR_NONE) 
	{
		Serial.println(F("Lora Initialized!"));

		display.clearBuffer();
		display.drawStr(0, 10, "Lora Initialized!");
		display.sendBuffer();
	} 
	else 
	{
		Serial.print(F("Lora failed, code "));
		Serial.println(state);

		display.clearBuffer();
		display.drawStr(0, 10, "Lora failed, code:" + state);
		display.sendBuffer();

		while (true);
	}

	if (Lora.setFrequency(915.0) == RADIOLIB_ERR_INVALID_FREQUENCY) 
	{
		Serial.println(F("Selected value is invalid for FREQUENCY"));
		while (true);
 	}

	if (Lora.setSpreadingFactor(7) == RADIOLIB_ERR_INVALID_SPREADING_FACTOR) 
	{
		Serial.println(F("Selected value is invalid for SPREADING_FACTOR!"));
		while (true);
	}

	if (Lora.setCodingRate(5) == RADIOLIB_ERR_INVALID_CODING_RATE) 
	{
		Serial.println(F("Selected value is invalid for CODING_RATE!"));
		while (true);
	}

	 // set the function that will be called
  	// when new packet is received
 	Lora.setDio1Action(setFlag);


    // send the first packet on this node
    Serial.println(F("[SX1262] Sending first packet ... "));

	display.clearBuffer();
	display.drawStr(0, 10, "Sending first packet..");
	display.sendBuffer();


  	String sendData = "hello #" + String(counter++);
    transmissionState = Lora.startTransmit(sendData) ; 
    transmitFlag = true;

	if (transmissionState == RADIOLIB_ERR_NONE) 
	{
		 Serial.print(F("success!"));
	}
	else
	{
		Serial.print(F("not success!"));
	}

}

void loop() 
{

	if(!operationDone)
	{
		delay(10);
		return;
	}

	int16_t rssi = 0;

	operationDone = false;

	if(transmitFlag)
	{
		Serial.println(F("Transmission finished: ")) ;
		transmitFlag = false;

		if (transmissionState == RADIOLIB_ERR_NONE)
		{
			Serial.print(F("with success!"));
		}
		else
		{
			Serial.print(F("with error: "));
			Serial.print(transmissionState);
		}

		Serial.println(F("Start receing data")) ;
		transmitFlag = false;
		Lora.startReceive();
		

	}
	else
	{
		Serial.println(F("Receiving finished: ")) ;

		String str;
      	int state = Lora.readData(str);

		if (state == RADIOLIB_ERR_NONE) 
		{
			Serial.print(F("with success!"));
			Serial.println(F("[SX1262] Data:\t\t"));
        	Serial.println(str);

			rssi = Lora.getRSSI();
			int16_t rssiDetection = abs(rssi);

			if(rssiDetection < 65)
			{
				digitalWrite(LED, HIGH);  
			}
			else
			{
				digitalWrite(LED, LOW);  
			}

			display.clearBuffer();
			
			display.drawStr(0, 10, "lora data show");
			display.drawStr(0, 20, String("R_data: " + str).c_str());
			display.drawStr(0, 30, String("R_size: " + (String)(str.length()) + " R_rssi:" + (String)(rssiDetection)).c_str());

			display.drawStr(0, 50, String("sent num: " + (String)(counter-1)).c_str());
			display.sendBuffer();
			delay(100); 

		}
		else
		{
			Serial.print(F("with error"));
		}

		delay(1000);		

		Serial.println(F("Start transmitting data")) ;
		String sendData = "hello #" + String(counter++) + " Rssi:" + (String)(rssi);
    	transmissionState = Lora.startTransmit(sendData) ; 

      	transmitFlag = true;
	}
}