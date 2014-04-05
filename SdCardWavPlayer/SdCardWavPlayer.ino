//The SD card is connected to the SPI with CS on pin 10.
//It does a file listing on startup...
//Send 'p' then the filename without .wav then LF

// Quick hardware test
#include <avr/io.h>
#include <avr/interrupt.h>
#include <SdFat.h>
// Test with reduced SPI speed for breadboards.
// Change spiSpeed to SPI_FULL_SPEED for better performance
// Use SPI_QUARTER_SPEED for even slower SPI bus speed
const uint8_t spiSpeed = SPI_FULL_SPEED;// SPI_HALF_SPEED;

const int ledPin = 2;
const int swPwm1Pin = 4;
const int swPwm2Pin = 5;
const int swPwm3Pin = 6;
const int swPwm4Pin = 7;
const int swPwm5Pin = 7;
const int swPwm6Pin = 7;
uint8_t pwmCounter = 0;

#define AUDIOBUFFER_LEN 256
#define AUDIOBUFFER_MASK 0xFF
#define AUDIOBUFFER_CHUNK 64

uint8_t audioBuffer[AUDIOBUFFER_LEN];
int16_t audioBufferBegin = 0;
int16_t audioBufferEnd = 0;

//------------------------------------------------------------------------------
// Normally SdFat is used in applications in place
// of Sd2Card, SdVolume, and SdFile for root.
Sd2Card card;
SdVolume volume;
SdFile root;
SdFile myFile;

// Serial streams
ArduinoOutStream cout(Serial);

// input buffer for line
char cinBuf[40];
ArduinoInStream cin(Serial, cinBuf, sizeof(cinBuf));

// SD card chip select
int chipSelect;

void cardOrSpeed() {
  cout << pstr(
    "Try another SD card or reduce the SPI bus speed.\n"
    "The current SPI speed is: ");
  uint8_t divisor = 1;
  for (uint8_t i = 0; i < spiSpeed; i++) divisor *= 2;
  cout << F_CPU * 0.5e-6 / divisor << pstr(" MHz\n");
  cout << pstr("Edit spiSpeed in this sketch to change it.\n");
}

void reformatMsg() {
  cout << pstr("Try reformatting the card.  For best results use\n");
  cout << pstr("the SdFormatter sketch in SdFat/examples or download\n");
  cout << pstr("and use SDFormatter from www.sdcard.org/consumer.\n");
}

uint16_t pwmTick = 0;
uint16_t swPwms[6];

uint8_t testCounter = 0;
ISR(TIMER2_OVF_vect)        // interrupt service routine 32Khz
{
  pwmCounter = pwmCounter ^ 1;
  
  //testCounter++;
  //OCR2B = testCounter;
  
  pwmTick++;
  if (pwmTick > 640)
    pwmTick = 0;
  
    digitalWrite(swPwm1Pin, (pwmTick < swPwms[0])?1:0 );
    digitalWrite(swPwm2Pin, (pwmTick < swPwms[1])?1:0 );
    digitalWrite(swPwm3Pin, (pwmTick < swPwms[2])?1:0 );
    
    /*
  if (pwmTick < swPwm1)
    digitalWrite(swPwm1Pin, 1);
  else
    digitalWrite(swPwm1Pin, 0);
  */
  if (pwmCounter == 0)
  {
    //TCNT1 = timer1_counter;   // preload timer
    
    digitalWrite(swPwm4Pin, (pwmTick < swPwms[4])?1:0 );
    digitalWrite(swPwm5Pin, (pwmTick < swPwms[5])?1:0 );
    
    
  }
  else
  {
      if (audioBufferBegin != audioBufferEnd)
      {
        OCR2B = audioBuffer[audioBufferBegin];
        audioBufferBegin = (audioBufferBegin + 1) & AUDIOBUFFER_MASK;
      }
      else
      {
        //digitalWrite(ledPin, digitalRead(ledPin) ^ 1);
      }
  }
  
}


void setup() 
{
  Serial.begin(9600);
  while (!Serial) {}  // wait for Leonardo
  
  swPwms[0] = 32;
  swPwms[1] = 32;
  swPwms[2] = 32;
  swPwms[3] = 32;
  swPwms[4] = 32;
  swPwms[5] = 32;
  
  pinMode(swPwm1Pin, OUTPUT);
  pinMode(swPwm2Pin, OUTPUT);
  pinMode(swPwm3Pin, OUTPUT);
  pinMode(swPwm4Pin, OUTPUT);
  pinMode(swPwm5Pin, OUTPUT);
  pinMode(swPwm6Pin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(3, OUTPUT);

  pinMode(A0, OUTPUT);
  pinMode(A1, OUTPUT);
  pinMode(A2, OUTPUT);
  pinMode(A3, OUTPUT);
  
  // initialize timer1 
  noInterrupts();           // disable all interrupts
  TCCR2A = 0x00;            
  TIFR2  = 0x00;
  TCCR2A = 0xA1;            // phase correct pwm on OC2B (digital 3)
  TCCR2B = 0x01;            // CK/1
  TIMSK2 = 1;   // enable overflow interrupt
  OCR2A = 0;
  OCR2B = 128;
  TCNT2 = 123;
  interrupts();             // enable all interrupts
}


#define SERIALBUFFER_SIZE 40
char serialBuffer[SERIALBUFFER_SIZE];
uint8_t serialBufferLen;

char audioFile[12];
#define DEFAULTFILE "B816.WAV"
char* defaultFile = DEFAULTFILE;

bool firstTry = true;
void loop() 
{
  for (int i=0; i<sizeof(DEFAULTFILE); i++)
    audioFile[i] = defaultFile[i];
  
  // read any existing Serial data
  while (Serial.read() >= 0) {}

  if (!firstTry) cout << pstr("\nRestarting\n");
  firstTry = false;

  chipSelect = 10;
  
  if (!card.init(spiSpeed, chipSelect)) {
    cout << pstr(
      "\nSD initialization failed.\n"
      "Do not reformat the card!\n"
      "Is the card correctly inserted?\n"
      "Is chipSelect set to the correct value?\n"
      "Is there a wiring/soldering problem?\n");
    cout << pstr("errorCode: ") << hex << showbase << int(card.errorCode());
    cout << pstr(", errorData: ") << int(card.errorData());
    cout << dec << noshowbase << endl;
    return;
  }
  cout << pstr("\nCard successfully initialized.\n");
  cout << endl;

  uint32_t size = card.cardSize();
  if (size == 0) {
    cout << pstr("Can't determine the card size.\n");
    cardOrSpeed();
    return;
  }
  uint32_t sizeMB = 0.000512 * size + 0.5;
  cout << pstr("Card size: ") << sizeMB;
  cout << pstr(" MB (MB = 1,000,000 bytes)\n");
  cout << endl;

  if (!volume.init(&card)) {
    if (card.errorCode()) {
      cout << pstr("Can't read the card.\n");
      cardOrSpeed();
    } else {
      cout << pstr("Can't find a valid FAT16/FAT32 partition.\n");
      reformatMsg();
    }
    return;
  }
  cout << pstr("Volume is FAT") << int(volume.fatType());
  cout << pstr(", Cluster size (bytes): ") << 512L * volume.blocksPerCluster();
  cout << endl << endl;

  if (!root.openRoot(&volume)) {
    cout << pstr("Can't open root directory.\n");
    reformatMsg();
    return;
  }
  cout << pstr("Files found (name date time size):\n");
  root.ls(LS_R | LS_DATE | LS_SIZE);

cout << pstr("exists: ") << root.exists(audioFile) << pstr("\n");
  
  myFile.open(&root, audioFile, O_READ);
  if (myFile.isOpen() == false)
  {
      cout << pstr("failed to open file\n");
  }

int16_t len;
int16_t data;
int16_t start;
bool isPlaying = true;

serialBufferLen = 0;

  while (true)
  {
    digitalWrite(ledPin, 1);

    if (isPlaying)
    {
      //Can we fit another chunk into the audio sample buffer
      len = (audioBufferEnd - audioBufferBegin) & AUDIOBUFFER_MASK;
      if (len < AUDIOBUFFER_LEN - AUDIOBUFFER_CHUNK - AUDIOBUFFER_CHUNK)
      {
        data = myFile.read(&audioBuffer[(audioBufferEnd + AUDIOBUFFER_CHUNK) & AUDIOBUFFER_MASK], AUDIOBUFFER_CHUNK);
        digitalWrite(ledPin, 0);
        
        if (data == AUDIOBUFFER_CHUNK)
        {
          //cout << audioBufferBegin << ' ' << audioBufferEnd << ' ' << data << '\n';
          audioBufferEnd = (audioBufferEnd + AUDIOBUFFER_CHUNK) & AUDIOBUFFER_MASK;
        }
        else
        {
          //We're done!
          isPlaying = false;
        }
      }
    }
    
    //check the serial port. Read into serialBuffer till we hit enter
    data = Serial.read();
    if (data > 0)
    {
      if (data != '\n')
      {
        if (serialBufferLen < SERIALBUFFER_SIZE)
        {
          serialBuffer[serialBufferLen] = (char)data;
          serialBufferLen++;
          
          //cout << (int)data << ' ' << (char)data << '\n';
        }
      }
      else
      {
        if (serialBuffer[0] == 'p')
        {
          for (data = 0; data < serialBufferLen-1; data++)
            audioFile[data] = serialBuffer[data+1];
          audioFile[serialBufferLen - 1] = '.';
          audioFile[serialBufferLen + 0] = 'W';
          audioFile[serialBufferLen + 1] = 'A';
          audioFile[serialBufferLen + 2] = 'V';          
          audioFile[serialBufferLen + 3] = '\0';          
          
          cout << pstr("new file: ") << audioFile << pstr(" \n");
          cout << pstr("exists: ") << root.exists(audioFile) << pstr("\n");
  
          myFile.close();
          myFile.open(&root, audioFile, O_READ);
          
          isPlaying = true;
        }  
        
        if ((serialBuffer[0] >= 'a') && (serialBuffer[0] <= 'e'))
        {
          swPwms[serialBuffer[0] - 'a'] = 16 + ((serialBuffer[1] - '0') * 10) + (serialBuffer[2] - '0');
        }  
  
        if ((serialBuffer[0] >= 'A') && (serialBuffer[0] <= 'G'))
        {
           digitalWrite(A0 + (serialBuffer[0] - 'A'), (serialBuffer[1] - '0')); 
        }
        
        serialBufferLen = 0;
      }
      
    }
  }
  
  cout << pstr("\nFINISHED\n");;

  myFile.close();
  root.close();
  
  

  // read any existing Serial data
  while (Serial.read() >= 0) {}
  cout << pstr("\nSuccess!  Type any character to restart.\n");
  while (Serial.read() < 0) {}
}
