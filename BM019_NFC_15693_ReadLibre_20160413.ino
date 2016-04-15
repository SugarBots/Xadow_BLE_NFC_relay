
#include <Boards.h>
#include <Firmata.h>
#include <SoftwareSerial.h>

/*
NFC Communication with the Solutions Cubed, LLC BM019 
and an Arduino Uno.  The BM019 is a module that
carries ST Micro's CR95HF, a serial to NFC converter.

Wiring:
 Arduino          BM019
 IRQ: Pin 9       DIN: pin 2
 SS: pin 10       SS: pin 3
 MOSI: pin 11     MOSI: pin 5 
 MISO: pin 12     MISO: pin4
 SCK: pin 13      SCK: pin 6


 Xdrip input from nightscout community
 data packet info from JoernL LimiTTer project

 This arduino project is purely experimental and should not be used to replace proven medical technologies and advice
 */

// the sensor communicates using SPI, so include the library:
#include <SPI.h>


byte my_Array[10][6];
int secondPass_Flag = 0;
int secondPass_Read = 0;
String fail_Read;
int get_Active_Offset = 0;
byte Active_Offset = 0;

const int SSPin = 10;  // Slave Select pin
const int IRQPin = 9;  // Sends wake-up pulse
byte TXBuffer[40];    // transmit buffer
byte RXBuffer[40];    // receive buffer
byte NFCReady = 0;  // used to track NFC state
byte Memory_Block = 0; // keep track of memory we write to
byte Data = 0; // keep track of memory we read from  
byte array_index = 0;
byte array_column = 0;  

char incoming_char = 0;

   SoftwareSerial myBLE(5, 4); // RX, TX BLE on pins D4, D5
   

void setup() {
    pinMode(IRQPin, OUTPUT);
    digitalWrite(IRQPin, HIGH); // Wake up pulse
    pinMode(SSPin, OUTPUT);
    digitalWrite(SSPin, HIGH);

   Serial.begin(9600);
   myBLE.begin(9600);
   delay(100);
   myBLE.print("AT+RESET");
   String myBLE_ID = ("xLibre");
   myBLE.print("AT+NAME" + myBLE_ID);
   //myBLE.print("AT+NAMExBridge%02x%02x");
  //"AT+NAMExBridge%02x%02x", serialNumber[0],serialNumber[1])
   
   Serial.println(myBLE_ID);
   
    myBLE.print("AT+ROLE0");
    myBLE.print("AT+PASS[00000]");
    myBLE.print("AT+AGLN[xLibre, 1]");
    Serial.println("Setting BLE");

    
    SPI.begin();
    SPI.setDataMode(SPI_MODE0);
    SPI.setBitOrder(MSBFIRST);
    SPI.setClockDivider(SPI_CLOCK_DIV32);
 
 // The CR95HF requires a wakeup pulse on its IRQ_IN pin
 // before it will select UART or SPI mode.  The IRQ_IN pin
 // is also the UART RX pin for DIN on the BM019 board.
 
    delay(10);                      // send a wake up
    digitalWrite(IRQPin, LOW);      // pulse to put the 
    delayMicroseconds(100);         // BM019 into SPI
    digitalWrite(IRQPin, HIGH);     // mode 
    delay(10);
}

/* SetProtocol_Command programs the CR95HF for
ISO/IEC 15693 operation.

This requires three steps.
1. send command
2. poll to see if CR95HF has data
3. read the response

If the correct response is received the serial monitor is used
to display successful programming. 
*/
void SetProtocol_Command()
 {
 byte i = 0;
 
// step 1 send the command
  digitalWrite(SSPin, LOW);
  SPI.transfer(0x00);  // SPI control byte to send command to CR95HF
  SPI.transfer(0x02);  // Set protocol command
  SPI.transfer(0x02);  // length of data to follow
  SPI.transfer(0x01);  // code for ISO/IEC 15693
  SPI.transfer(0x0D);  // Wait for SOF, 10% modulation, append CRC
  digitalWrite(SSPin, HIGH);
  delay(1);
 
// step 2, poll for data ready

  digitalWrite(SSPin, LOW);
  while(RXBuffer[0] != 8)
    {
    RXBuffer[0] = SPI.transfer(0x03);  // Write 3 until
    RXBuffer[0] = RXBuffer[0] & 0x08;  // bit 3 is set
    }
  digitalWrite(SSPin, HIGH);
  delay(1);

// step 3, read the data
  digitalWrite(SSPin, LOW);
  SPI.transfer(0x02);   // SPI control byte for read         
  RXBuffer[0] = SPI.transfer(0);  // response code
  RXBuffer[1] = SPI.transfer(0);  // length of data
  digitalWrite(SSPin, HIGH);

  if ((RXBuffer[0] == 0) & (RXBuffer[1] == 0))  // is response code good?
    {
    Serial.println("Protocol Set Command OK");
    NFCReady = 1; // NFC is ready
    }
  else
    {
    Serial.println("Protocol Set Command FAIL");
    NFCReady = 0; // NFC not ready
    }
}

/* Inventory_Command checks to see if an RF tag is in range of the BM019.

This requires three steps.
1. send command
2. poll to see if CR95HF has data
3. read the response

If the correct response is received the serial monitor is used
to display the the RF tag's universal ID.  
*/
void Inventory_Command()
 {
 byte i = 0;

// step 1 send the command
  digitalWrite(SSPin, LOW);
  SPI.transfer(0x00);  // SPI control byte to send command to CR95HF
  SPI.transfer(0x04);  // Send Receive CR95HF command
  SPI.transfer(0x03);  // length of data that follows is 0
  SPI.transfer(0x26);  // request Flags byte
  SPI.transfer(0x01);  // Inventory Command for ISO/IEC 15693
  SPI.transfer(0x00);  // mask length for inventory command
  digitalWrite(SSPin, HIGH);
  delay(1);
 
// step 2, poll for data ready
// data is ready when a read byte
// has bit 3 set (ex:  B'0000 1000')

  digitalWrite(SSPin, LOW);
  while(RXBuffer[0] != 8)
    {
    RXBuffer[0] = SPI.transfer(0x03);  // Write 3 until
    RXBuffer[0] = RXBuffer[0] & 0x08;  // bit 3 is set
    }
  digitalWrite(SSPin, HIGH);
  delay(1);


// step 3, read the data
  digitalWrite(SSPin, LOW);
  SPI.transfer(0x02);   // SPI control byte for read         
  RXBuffer[0] = SPI.transfer(0);  // response code
  RXBuffer[1] = SPI.transfer(0);  // length of data
  for (i=0;i<RXBuffer[1];i++)      
      RXBuffer[i+2]=SPI.transfer(0);  // data
  digitalWrite(SSPin, HIGH);
  delay(1);

  if (RXBuffer[0] == 128)  // is response code good?
    {

    Serial.println(" ");
    Serial.println("TAG DETECTED");
    Serial.print("UID: ");
    for(i=11;i>=3;i--)
    {
      //Serial.print(RXBuffer[i],HEX);
      print_hex(RXBuffer[i]);
      Serial.print(" ");
    }
    Serial.println(" ");
    Serial.println("Inventory Command OK");
    NFCReady = 2;
    }
  else
    {
    Serial.println("Inventory Command FAIL");
    NFCReady = 0;
    }
 }
 
/* Write Memory writes data to a block of memory.  
This code assumes the RF tag has less than 256 blocks
of memory and that the block size is 4 bytes.  The block 
written to increments with each pass and if it exceeds
20 it starts over. Data is also changed with each write.

This requires three steps.
1. send command
2. poll to see if CR95HF has data
3. read the response
*/
void Write_Memory()
 {
 byte i = 0;
    
  Data = Data +4;
  Memory_Block = Memory_Block +1;
  if (Memory_Block > 39)
    Memory_Block = 0;
  
// step 1 send the command
  digitalWrite(SSPin, LOW);
  SPI.transfer(0x00);  // SPI control byte to send command to CR95HF
  SPI.transfer(0x04);  // Send Receive CR95HF command
  SPI.transfer(0x07);  // length of data that follows 
//  SPI.transfer(0x08);  //Use this length if more than 256 mem blocks 
  SPI.transfer(0x02);  // request Flags byte
//  SPI.transfer(0x0A);  // request Flags byte if more than 256 memory blocks
  SPI.transfer(0x21);  // Write Single Block command for ISO/IEC 15693
  SPI.transfer(Memory_Block);  // Memory block address
//  SPI.transfer(0x00);  // MSB of mem. address greater than 256 blocks
  SPI.transfer(Data);  // first byte block of memory block
  SPI.transfer(Data+1);  // second byte block of memory block
  SPI.transfer(Data+2);  // third byte block of memory block
  SPI.transfer(Data+3);  // third byte block of memory block
  digitalWrite(SSPin, HIGH);
  delay(1);
 
// step 2, poll for data ready
// data is ready when a read byte
// has bit 3 set (ex:  B'0000 1000')

  digitalWrite(SSPin, LOW);
  while(RXBuffer[0] != 8)
    {
    RXBuffer[0] = SPI.transfer(0x03);  // Write 3 until
    RXBuffer[0] = RXBuffer[0] & 0x08;  // bit 3 is set
    }
  digitalWrite(SSPin, HIGH);
  delay(1);


// step 3, read the data
  digitalWrite(SSPin, LOW);
  SPI.transfer(0x02);   // SPI control byte for read         
  RXBuffer[0] = SPI.transfer(0);  // response code
  RXBuffer[1] = SPI.transfer(0);  // length of data
  for (i=0;i<RXBuffer[1];i++)      
      RXBuffer[i+2]=SPI.transfer(0);  // data
  digitalWrite(SSPin, HIGH);
  delay(1);

  if (RXBuffer[0] == 128)  // is response code good?
    {
    //Serial.print("Write Memory Block Command OK - Block ");
    //Serial.print(Memory_Block);
    //Serial.print(":  ");
    //Serial.print(Data);
    //Serial.print(" ");
    //Serial.print(Data+1);
    //Serial.print(" ");
    //Serial.print(Data+2);
    //Serial.print(" ");
    //Serial.print(Data+3);
    //Serial.println(" ");
    NFCReady = 2;
    }
  else
    {
    Serial.print("Write Memory Block Command FAIL");
    NFCReady = 0;
    }
 }


void print_hex(int val)
{
char str[20];
//sprintf(str,"%04x",val);
sprintf(str,"%02x",val);
//Serial.print("serialprint using print_hex function : ");
Serial.print(str);
}

/* Read Memory reads data from a block of memory.  
This code assumes the RF tag has less than 256 blocks
of memory and that the block size is 4 bytes.  Data
read from is displayed via the serial monitor.

This requires three steps.
1. send command
2. poll to see if CR95HF has data
3. read the response
*/
void Read_Memory()
 {
 byte i = 0;
   
// step 1 send the command
  digitalWrite(SSPin, LOW);
  SPI.transfer(0x00);  // SPI control byte to send command to CR95HF
  SPI.transfer(0x04);  // Send Receive CR95HF command
  SPI.transfer(0x03);  // length of data that follows
 // SPI.transfer(0x04);  // length of data if mem > 256
  SPI.transfer(0x02);  // request Flags byte
 // SPI.transfer(0x0A);  // request Flags byte if mem > 256
  SPI.transfer(0x20);  // Read Single Block command for ISO/IEC 15693
  SPI.transfer(Memory_Block);  // memory block address
//  SPI.transfer(0x00);  // MSB of memory block address if mem > 256
  digitalWrite(SSPin, HIGH);
  delay(1);
 
// step 2, poll for data ready
// data is ready when a read byte
// has bit 3 set (ex:  B'0000 1000')

  digitalWrite(SSPin, LOW);
  while(RXBuffer[0] != 8)
    {
    RXBuffer[0] = SPI.transfer(0x03);  // Write 3 until
    RXBuffer[0] = RXBuffer[0] & 0x08;  // bit 3 is set
    }
  digitalWrite(SSPin, HIGH);
  delay(1);


// step 3, read the data

  digitalWrite(SSPin, LOW);
  SPI.transfer(0x02);   // SPI control byte for read         
  RXBuffer[0] = SPI.transfer(0);  // response code
  RXBuffer[1] = SPI.transfer(0);  // length of data
  
  for (i=0;i<RXBuffer[1];i++)      
      RXBuffer[i+2]=SPI.transfer(0);  // data
  digitalWrite(SSPin, HIGH);
  delay(1);

  //start reading from block 3

  
  if (RXBuffer[0] == 128) // is response code good?
    {
      if (get_Active_Offset == 1)
        {
          Serial.print("Reading Active Offset ");
          print_hex(RXBuffer[5]);
          Serial.println();
          Active_Offset = RXBuffer[5];       
          my_Array[0][0] = RXBuffer[7];
          my_Array[0][1] = RXBuffer[8];       
          array_column = 0;
          array_index = 1;
        }
        else
        {
       //Serial.println("Populating data from previous n readings");
       //first deal with tail end of block 3
       
       //now populate the rest of the active block  
       
          for (int j = 3; j<=10; j++)
         {
          if(array_column ==6)
          {
              array_column = 0;
              array_index = array_index+1;
          }

            my_Array[array_index][array_column] = RXBuffer[j];
            array_column = array_column+1;
         }
      
      NFCReady = 2;
    }
    }
  else
    {
    Serial.print("Read Memory Block Command FAIL");
         if (secondPass_Flag == 0)
            {
              fail_Read = fail_Read + Memory_Block;
              fail_Read = fail_Read + ";";
            }
    NFCReady = 0;
    }
 }

 void Interpret_Tag()
 {
  byte rawBG;
  float BloodGlucose;
  float xBG_raw;
  String bgConstant_String = "H1FFF";
  int bgConstant = 8191;
  

int Offset = Active_Offset;

  //rawBG = my_Array[Offset-1][1] + my_Array[Offset-1][0];
  long rawBG_Val1 = my_Array[Offset - 1][1];
  
  Serial.println(rawBG_Val1);
  rawBG_Val1 = abs(rawBG_Val1 * 256);
  Serial.print(" Val 1 x 256");
  Serial.println(rawBG_Val1);
     
  long rawBG_Val2 = my_Array[Offset - 1][0];
  rawBG_Val2 = abs(rawBG_Val2 * 1);
  Serial.print ("RawBG Val2");
  Serial.println(rawBG_Val2);

  long rawBG_Val = rawBG_Val1 + rawBG_Val2;
  Serial.print ("RawBG Combo");
  Serial.println(rawBG_Val);
  
  int rawBG_Val_Int = rawBG_Val;

  rawBG_Val_Int = rawBG_Val_Int & bgConstant;
  
 BloodGlucose = rawBG_Val_Int;
 BloodGlucose = BloodGlucose / 10;
 Serial.println("Calculated Glucose in mmol: ");
 Serial.println(BloodGlucose,1);
 //myBLE.println("Sending Data");
 xBG_raw = BloodGlucose*18.1;
 xBG_raw = xBG_raw*733;
 
 char str_temp[7];
 dtostrf(xBG_raw, 6, 0, str_temp);
 Serial.println("Calculated Glucose in mgdl: ");
 Serial.println(str_temp);

 char buffer[14];
 //sprintf( buffer,"%02x 00x0 %lu %lu %02x %02x %lu %02x",myPacket.size, myPacket.raw, myPacket.filtered, myPacket.dex_battery, myPacket.my_battery, myPacket.dex_src_id, myPacket.function);
 sprintf(buffer,"%s 213 175",str_temp);// 213 is battery OK value
 myBLE.println(buffer);
 Serial.println(buffer);
 }
 
 String Read_SoftSerial(SoftwareSerial inputSerial)
 {
 String returnValue;
 if(inputSerial.available() > 0)
  {
    incoming_char=inputSerial.read();
    if((incoming_char >= ' ') && (incoming_char <= 'z'))
      returnValue +=(incoming_char);
    else
    {
      returnValue += ("%");
      returnValue += ((int)incoming_char);
      returnValue += ("%");
      if(incoming_char==10)
        returnValue += ("/n");
    }
    return returnValue;
  }
 }


 String Read_Serial()
 {
 String readString;
 
   while (Serial.available()) 
   {
    delay(3);  
    char c = Serial.read();
    readString += c; 
  }
  readString.trim();
  return readString;
}
 
void waitForResponse(SoftwareSerial mySoftSerial)
{
delay(1000); 
while (mySoftSerial.available()) 
  {
  Serial.write(mySoftSerial.read());
  }
Serial.write("\n");
}

void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

 
void loop() {
  String fail_Read;
  int secondPass_Flag = 0;
  int secondPass_Read = 0;

   
  if(NFCReady == 0)
    {
    delay(100);  
    SetProtocol_Command(); // ISO 15693 settings
    }
  else if (NFCReady == 1)
    {
    delay(100);  
    Inventory_Command();
    }
  else
    {
    //Write_Memory();
    //delay(100); 
    get_Active_Offset = 1;
    Memory_Block = 3;
    Read_Memory();
    delay(300);
    Memory_Block = Active_Offset - 1;
    get_Active_Offset = 0;
    for (Memory_Block = 4; Memory_Block <=15; Memory_Block++)
    {
    Read_Memory();
    delay(100);
    }
    
    Serial.println(" ");
    Interpret_Tag();
    
    delay(10000 * 3); //set timer on 5-minute loop
    }  
    delay(100 * 100); 
}
