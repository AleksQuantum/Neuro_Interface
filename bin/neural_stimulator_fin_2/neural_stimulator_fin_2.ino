#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif

#define TEST_PROG_VERSION "v1.2"

#include <Stream.h>
#include <SPI.h>
#include <Wire.h>
#include <ADG2128.h>
#if defined(ADG2128_DEBUG)
  // If debugging is enabled in the build, another dependency will be needed.
  // It can be disabled in the driver's header file.
  // https://github.com/jspark311/CppPotpourri
  #include <StringBuilder.h>
#endif  // ADG2128_DEBUG
#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOS.h>

// Define buffer size
#define SERIAL_BUFFER_SIZE 128

/*******************************************************************************
* Pin definitions and hardware constants.
*******************************************************************************/
#define CS_PIN              0
#define MISO_PIN            20
#define MOSI_PIN            1
#define CLK_PIN             2

#define SDA_PIN             22
#define SCL_PIN             23
#define RESET_PIN           16


/*******************************************************************************
* Globals
*******************************************************************************/
ADG2128 adg2128(ADG2128_DEFAULT_I2C_ADDR, RESET_PIN);
uint8_t selected_row = 0;
uint8_t selected_col = 0;
bool    row_col_opt  = false;

uint8_t selectRow(char row) {
  return (row < 'A') ? (row - 0x30) : (row - 0x37);
}
uint8_t selectCol(char col) {
  return (col - 0x30);
}

bool check = false;

uint8_t channel = 1;

uint64_t signal_pos_delay = 10000;
uint64_t signal_neg_delay = 10000;
uint64_t delay_between_signals = 1000;
uint64_t general_delay = 1000;
uint64_t repeat_signal = 0;

uint8_t potentiometer_state_pos = 0;
uint8_t potentiometer_state_neg = 0;

uint8_t potentiometer_state_prev = 0;

String signal_attributes[9];

void microDelay(uint64_t number_of_microseconds) {
  uint64_t microseconds = esp_timer_get_time();

  if (0 != number_of_microseconds)
  {
    while (((uint64_t) esp_timer_get_time() - microseconds) <=
            number_of_microseconds)
    {
        // Wait
    }
  }
}

bool hasHyphens(const String& str) {
    int count = 0;
    for(char c : str) {
        if(c == '-') {
            count++;
            if(count >= 8) {
                return true;
            }
        }
    }
    return false;
}

int splitByHyphen(String input, String output[]) {
  int maxParts = 9;
  int currentPos = 0;
  int foundPos = -1;
  int partIndex = 0;

  while (partIndex < maxParts - 1) { // Need to find maxParts - 1 hyphens
    foundPos = input.indexOf('-', currentPos);
    if (foundPos == -1) {
      break; // No more hyphens found
    }
    // Extract substring from currentPos to foundPos
    output[partIndex] = input.substring(currentPos, foundPos);
    partIndex++;
    // Update currentPos to character after the found hyphen
    currentPos = foundPos + 1;
  }

  // After the loop, extract the remaining part of the string
  output[partIndex] = input.substring(currentPos);
  partIndex++;

  return partIndex;
}

bool isNumeric(const String& str) {
  if (str.length() == 0) {
    return false; // Empty string is not considered numeric
  }
  for (unsigned int i = 0; i < str.length(); i++) {
    if (!isDigit(str[i])) {
      return false; // Found a non-digit character
    }
  }
  return true; // All characters are digits
}

uint64_t stringToUint64(const String& str) {
  // Convert the String to a long integer
  long value = str.toInt();

  return static_cast<uint64_t>(value);
}

uint8_t stringToUint8(const String& str) {
  // Convert the String to a long integer
  long value = str.toInt();

  // Clamp the value to ensure it's within 0 to 254
  if (value < 0) {
    value = 0;
  } else if (value > 254) {
    value = 254;
  }

  return static_cast<uint8_t>(value);
}

void setAD5160Resistance(byte value) {
  /*--- Function to set the resistance value on the AD5160 ---*/
  digitalWrite(CS_PIN, LOW);      // Select the AD5160
  //delay(500);
  SPI.transfer(0x11);                         // Command byte to update the resistance
  SPI.transfer(value);                        // Send the value to set the resistance (0 to POT_MAX)
  digitalWrite(CS_PIN, HIGH);     // Deselect the AD5160
}

void SendSignal(void *parameter)
{
  while(1){
    while(repeat_signal > 0) {
      Serial.println(check);
      if (check) {
        char ch1 = '0';
        char ch2 = '11';
        switch (channel)
        {
          case 1:
            ch1 = '0';
            ch2 = '11';
            break;
          case 2:
            ch1 = '1';
            ch2 = '10';
            break;
          case 3:
            ch1 = '2';
            ch2 = '9';
            break;
          case 4:
            ch1 = '3';
            ch2 = '8';
            break;
          case 5:
            ch1 = '4';
            ch2 = '7';
            break;
          case 6:
            ch1 = '5';
            ch2 = '6';
            break;
        }
        Serial.println("Ping!");
        Serial.println(ch1);
        Serial.println(ch2);
        //microDelay(signal_pos_delay);
        //microDelay(delay_between_signals);
        //microDelay(signal_neg_delay);
        //microDelay(general_delay);

        if (potentiometer_state_pos != potentiometer_state_prev) {
          setAD5160Resistance(potentiometer_state_pos);
          potentiometer_state_prev = potentiometer_state_pos;
        }

        adg2128.setRoute(selectCol('0'), selectRow(ch1));
        adg2128.setRoute(selectCol('7'), selectRow(ch2));
        microDelay(signal_pos_delay);
        adg2128.unsetRoute(selectCol('0'), selectRow(ch1));
        adg2128.unsetRoute(selectCol('7'), selectRow(ch2));

        microDelay(delay_between_signals);

        if (potentiometer_state_neg != potentiometer_state_prev) {
          setAD5160Resistance(potentiometer_state_neg);
          potentiometer_state_prev = potentiometer_state_neg;
        }

        adg2128.setRoute(selectCol('0'), selectRow(ch2));
        adg2128.setRoute(selectCol('7'), selectRow(ch1));
        microDelay(signal_neg_delay);
        adg2128.unsetRoute(selectCol('0'), selectRow(ch2));
        adg2128.unsetRoute(selectCol('7'), selectRow(ch1));
        microDelay(general_delay);
        
        Serial.println("Ping2!");
      }
      repeat_signal--;
      if (repeat_signal < 1) { Serial.println("Signal Disabled."); }
    }
  }
}

// Queue to send complete strings to other tasks
QueueHandle_t serialQueue;

// Serial reading task
void SerialReadTask(void *pvParameters)
{
    char serialBuffer[SERIAL_BUFFER_SIZE];
    uint16_t bufferIndex = 0;

    for (;;)
    {
        if (Serial.available() > 0)
        {
            char receivedByte = Serial.read();

            if (receivedByte == '\n' || receivedByte == '\r')
            {
                if (bufferIndex > 0)
                {
                    serialBuffer[bufferIndex] = '\0'; // Null-terminate the string

                    // Allocate memory for the string
                    char *completeString = (char *)malloc(bufferIndex + 1);
                    if (completeString != NULL)
                    {
                        strcpy(completeString, serialBuffer);
                        xQueueSend(serialQueue, &completeString, portMAX_DELAY);
                    }

                    // Reset buffer index
                    bufferIndex = 0;
                }
            }
            else
            {
                if (bufferIndex < (SERIAL_BUFFER_SIZE - 1))
                {
                    serialBuffer[bufferIndex++] = receivedByte;
                }
                else
                {
                    // Buffer overflow handling
                    bufferIndex = 0;
                }
            }
        }

        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Processing task
void ProcessSerialTask(void *pvParameters)
{
    char *receivedString;

    for (;;)
    {
        if (xQueueReceive(serialQueue, &receivedString, portMAX_DELAY) == pdTRUE)
        {
            // Process the received string
            Serial.print("Received: ");
            Serial.println(receivedString);

            // put your main code here, to run repeatedly:
            //Serial.println("Enter command:");
            // while (Serial.available() == 0) {}     //wait for data available
            //String command = Serial.readString();  //read until timeout

            String command = String(receivedString);
            // Free the allocated memory
            free(receivedString);

            command.trim();                        // remove any \r \n whitespace at the end of the String
            Serial.println(command);

            if (command[0] == 'e') {
              check = true;
              Serial.println("Signal Enabled.");
            }
            if (command[0] == 'd') {
              check = false;
              Serial.println("Signal Disabled.");
              continue;
            }

            if (hasHyphens(command)) {
              Serial.println("Parsing...");
              int numSubstrings = splitByHyphen(command, signal_attributes);
              /*if (numSubstrings == 9) {
                Serial.println("Successfully split the string into 9 substrings:");
                for (int i = 0; i < 9; i++) {
                  Serial.print("Substring ");
                  Serial.print(i + 1);
                  Serial.print(": ");
                  Serial.println(signal_attributes[i]);
                }
              }*/
              if (isNumeric(signal_attributes[1]))
              {
                channel = stringToUint8(signal_attributes[1]);
                Serial.println("Channel:");
                Serial.println(channel);
              }
              if (isNumeric(signal_attributes[2]))
              {
                signal_pos_delay = stringToUint64(signal_attributes[2]);
                Serial.println("Positive delay:");
                Serial.println(signal_pos_delay);
              }
              if (isNumeric(signal_attributes[3]))
              {
                potentiometer_state_pos = stringToUint8(signal_attributes[3]);
                Serial.println("Positive potentiometer state:");
                Serial.println(potentiometer_state_pos);
              }
              if (isNumeric(signal_attributes[4]))
              {
                delay_between_signals = stringToUint64(signal_attributes[4]);
                Serial.println("Delay between signals:");
                Serial.println(delay_between_signals);
              }
              if (isNumeric(signal_attributes[5]))
              {
                signal_neg_delay = stringToUint64(signal_attributes[5]);
                Serial.println("Negative delay:");
                Serial.println(signal_neg_delay);
              }
              if (isNumeric(signal_attributes[6]))
              {
                potentiometer_state_neg = stringToUint8(signal_attributes[6]);
                Serial.println("Negative potentiometer state:");
                Serial.println(potentiometer_state_neg);
              }
              if (isNumeric(signal_attributes[7]))
              {
                general_delay = stringToUint64(signal_attributes[7]);
                Serial.println("General delay:");
                Serial.println(general_delay);
              }
              if (isNumeric(signal_attributes[8]))
              {
                repeat_signal = stringToUint64(signal_attributes[8]);
                Serial.println("Ammout of repeats:");
                Serial.println(repeat_signal);
              }
            } else {
              Serial.println("Wrong command.");
            }
        }
    }
}

void setup()
{
    pinMode(CS_PIN, OUTPUT);
    digitalWrite(CS_PIN, HIGH); // turn spi device 1 off initially

    Serial.begin(115200);
    SPI.begin(CLK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);
    setAD5160Resistance(0);

    Wire.setPins(SDA_PIN, SCL_PIN);
    Wire.begin();

    ADG2128_ERROR ret = ADG2128_ERROR::NO_ERROR;
    ret = adg2128.init(&Wire);
    Serial.println(ADG2128::errorToStr(ret));

    // Create the queue
    serialQueue = xQueueCreate(10, sizeof(char *));

    xTaskCreate(
        SendSignal, 
        "SendSignal", 
        1024, 
        NULL, 
        1, 
        NULL
    );

    // Create the serial read task
    xTaskCreate(
        SerialReadTask,
        "SerialReadTask",
        1024,
        NULL,
        1,
        NULL
    );

    // Create the processing task
    xTaskCreate(
        ProcessSerialTask,
        "ProcessSerialTask",
        1024,
        NULL,
        1,
        NULL
    );
}

void loop()
{
    // Empty. Tasks are handled by FreeRTOS.
}




