/**
 * 
 алгоритм работы пульта PRO V2
 
шифт + 1 банк = первый суб банк.
шифт + 2 банк = второй суб банк 
шифт + 3 банк = посыл сс вместе с нотами
Фиксация шифта = сс вместо нот.
Дабл шифт = фиксация шифта програмно. Отключение - однократный короткий шифт.

шифт +4 банк = включение датчика
отключается датчик простым нажатием на шифт, и при отключении он должен дать 127. 

удержанный шифт при включении активирует эко режим.Фиксация шифта = сс вместо нот.
Дабл шифт = фиксация шифта програмно. Отключение - однократный короткий шифт.

Шифт во время не нулевого питча = питч фиксация

===
джойстик 
имеет 3 режима:
1) влево вправо analogCC вверх вниз питч
2) вверх вниз влево вправо analog CC
3) вверх вниз влево вправо digitalCC

режимы  переключаются вверх длительным нажатием. очень длительное нажатие переключит из первого в третий режим
режимы  переключаютсявниз - коротким нажатием. 
длительное удержание в третьем режиме ничего не даст.
короткое нажатие в первом режиме - send cc ( либо фикс питч но это беда с джойстиком механическая)


HOW TO CODE 

в свиче шифта после фиксации любого нажатия (шифт короткий, долгий, даблшифт) уходим на кейс отработки логики и там уже уже проверяем 
if(doubleshift) { 
	doubleshift = 0; 
	sendMode = notes;
} else send cc;


в остальных падах после зажатия шифта выставляем флаг shifthold
при опросе нажатий на кнопки в случае верхних четырех
if(shifthold){
	~~ changeMode ~~
}
при опросе обычных падов
if(shifthold){
	~~ send note ~~
} else {
~~ sendCC ~~
}

 * 
 * 
 * 
 */

 
 

#include <MIDIUSB.h>
#include <ResponsiveAnalogRead.h>
//#include <MIDI.h>
//#include "ClickEncoder.h"
//#include <TimerOne.h>

//MIDI_CREATE_DEFAULT_INSTANCE();




#define DEBUGJOY // отладка джойстика
#define JOYCENTERCORR 10
#define JOYEDGECORR 15

#define LONGPUSHTIME 500 // время в милисекундах за которое программа сочтет, что кнопка нажата длительно




//-----------------------------Матричная клавиатура---------------------

const byte ROWS = 5; //Количество рядов матричной клавиатуры
const byte COLS = 7; //Количество столбцов матричной клавиатуры
const byte KBD_COUNT = 16;
const byte ENCODERS = 5;
const byte ANALOGS = 4;

enum {
  NOTE_OFFSET_NONE = 0,
  NOTE_OFFSET_GHOST_BANK = -4,
};

enum {
  MIDI_CHANNEL_MICRO = 0,
  MIDI_CHANNEL_MINI  = 1,
  MIDI_CHANNEL_PRO   = 2,
};

byte rowPins[ROWS] = {17, 2, A2, 1, 7}; // Пины рядов
//byte colPins[COLS] = {2, 3, 4, 5, 6, 7, 8}; // Пины столбцов


// Пины для сдвигового регистра
enum {
  SHIFT_DATA_PIN  = 16,    // пин, передающий данные, DAT
  SHIFT_CLOCK_PIN = 15,    // пин, управляющий синхронизацией, SCK
  SHIFT_LATCH_PIN = 14,    // пин, управляющий защёлкой (SS в терминах SPI), STB
};
// Ещё зарезервированы для сдвигового регистра
// 12
// 13
// Их нельзя использовать в других целях

enum {
  RED_PIN = 9,
  GREEN_PIN = 5,
  BLUE_PIN = 13,
};

enum {
  JOYSTICK_RED_PIN = 11,
  JOYSTICK_GREEN_PIN = 3,
  JOYSTICK_BLUE_PIN = 10,
};

// автоматы обработки каждой кнопки
byte button[ROWS][COLS] = {
  {1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1}
};
// массив фактического не обработанного от дребезга состояния кнопки
byte keyRawState[ROWS][COLS] = {
  {1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1}
};

// Массив пользовательского (уже отфильтрованного) состояния кнопок
byte keypadState[ROWS][COLS] = {
  {1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1}
};

// массив используется для отладки
byte keypadStatePrev[ROWS][COLS] = { 
  {1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1}
};

// переменные счетчиков времени для работы автоматов
unsigned long keyCurMs[ROWS][COLS] = { 
  {1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1},
  {1, 1, 1, 1, 1, 1, 1}
};

// Массив нот
byte kpdNote[4][4] = {
  {48, 49, 50, 51},
  {44, 45, 46, 47},
  {40, 41, 42, 43},
  {36, 37, 38, 39}
};

byte noteOffset = NOTE_OFFSET_NONE; // =0

byte kpdCc[4][4] = {
  {16, 17, 18, 19},
  {20, 21, 22, 23},
  {24, 25, 26, 27},
  {28, 29, 30, 31}
};

byte kpdDoubleCc[4][4] = {
  {70, 71, 72, 73},
  {74, 75, 76, 77},
  {78, 79, 80, 81},
  {82, 83, 84, 85}
};

// Номера ЦЦ, которые посылаются при смене банка
byte bankChangeCc[ROWS] = {12, 13, 14, 15};

// Номер текущего банка
byte bank = 0;

// Включён ли режим Шифт/Сдвига?
bool isShift = false;

// Включён ли режим Нота+ЦЦ
bool isDouble = false;


byte midiChannel = MIDI_CHANNEL_PRO;

byte bankColors[7][3] = {
  {HIGH, LOW,  HIGH}, // фиолетовый = красный + синий
  {LOW,  LOW,  HIGH}, // синий
  {LOW,  HIGH,  LOW}, // зелёный
  {HIGH, LOW,   LOW}, // красный
  {HIGH, HIGH, HIGH}, // белый = красный + зелёный + синий
  {LOW,  HIGH, HIGH}, // бирюзовый = зелёный + синий
  {HIGH, HIGH,  LOW}  // жёлтый = красный + зелёный
};

//----------------------------------------------------------------------

// время от старта программы
unsigned long ms = 0;
// Текущее время цикла
unsigned long currentTime[ENCODERS];
// Предыдущее время цикла
unsigned long loopTime[ENCODERS];
// Время последнего изменения значения энкодера
unsigned long msEncoderChangeTime[ENCODERS];

byte encoderA[ENCODERS];
byte encoderB[ENCODERS];
byte encoderAprev[ENCODERS] = {0, 0, 0, 0, 0};

int encoderPos[ENCODERS]    = {0, 0, 0, 0, 0};
int encoderPrevPos[ENCODERS] = {0, 0, 0, 0, 0};
byte encoderCc[ENCODERS] = {106, 105, 104, 103, 102};

byte encoderACol = 7;
byte encoderBCol = 6;
byte encoderButtonCol = 5;

byte encoderButtonCc[ENCODERS] = {28, 29, 30, 31}; 	// нигде не используется
byte encoderButtonState[ENCODERS] = {0, 0, 0, 0}; 	// нигде не используется

// Если предыдущее значение энкодера было старше этого интервала, изменяем значение энкодера плавно
unsigned int msEncoderSlowDelay = 90;
unsigned int msEncoderMidDelay = 65;
// Шаги изменения значения энкодеров: быстро, средне, медленно
byte encoderStep = 6;
byte encoderMidStep = 3;
byte encoderSlowStep = 1;

// https://forum.arduino.cc/index.php?topic=241369.0
byte analogPins[ANALOGS] = {A5, A4, 4, 8};
ResponsiveAnalogRead analogs[ANALOGS] = {
  ResponsiveAnalogRead(A5,  true, 0.0001),
  ResponsiveAnalogRead(A4,  true, 0.0001),
  ResponsiveAnalogRead(A6, true, 0.0001), // arduino пин 4, пин атмеги 25
  ResponsiveAnalogRead(A8, true, 0.0001), // arduino пин 8, пин атмеги 28
};

byte analogCc[ANALOGS] = {63, 62, 61, 60};
int analogPrevPos[ANALOGS] = {-1, -1, -1, -1};
byte analogStep = 7;
unsigned long currentAnalogTime[ENCODERS];
unsigned long loopAnalogTime[ENCODERS];
const byte ANALOG_SAMPLES = 40;
int analogSampleCounts[ANALOGS] = {0, 0, 0, 0};
double analogSampleSums[ANALOGS] = {0, 0, 0, 0};

byte val = 0;
byte dval = 0;


// Инфракрасный датчик близости
// Дальномер
bool irActive = false;
byte irPin = A3;
byte irCc = 57;
int irPrevPos = -1;
ResponsiveAnalogRead ir(irPin, true, 0.005);

const short IR_VALUE_MIN = 130;
const short IR_VALUE_MAX = 640;

const byte IR_SAMPLES = 60;
int previousDistance;
byte sampleIndex = 0;
byte sampleCount = 0;
double sampleSum = 0;
float irTolerance = 0.93;



// Джойстик
byte joyXPin = A11; // пин  джойстика вверх вниз, ось X
byte joyYPin = A7; // пин Джойстика влево-вправо, ось y
byte joyButtonPin = A0;
// del me byte joyButtonCc = 7;
// del me byte joyButtonPrev = HIGH;

//настройка питча 
const unsigned int PITCH_BEND_MIN = 0;
const unsigned int PITCH_BEND_CENTER = 0x2000;
const unsigned int PITCH_BEND_MAX = 16383;

// это по фильтрации сигнала с джойстика 
short joyXPrev = PITCH_BEND_CENTER;
short XBendPrev = PITCH_BEND_CENTER;
ResponsiveAnalogRead joyXanalog(joyXPin, true, 0.001);
short valX = 0; // полученное с ResponsiveAnalogRead фильтра значение 
double joyYfilter = 0.0;
// Если будет сильно дребезжать влево-вправо, поставить 0.0001
ResponsiveAnalogRead joyYanalog(joyYPin, true, 0.001);
short valY = 0; // полученное с ResponsiveAnalogRead фильтра значение 

///////////////////////////////АВТОМАТЫ/////////////////////////////////////////////////////////
// 
// автомат обработки X(вертикали) джойтсика
byte joystickX = 0;
byte joystickXPrevious = 0; // для отладкии

//unsigned long joystickXMs = 0;
//int joystickXMsDel = 0; 

// автомат обработки Y(горизонтали) джойтсика
byte joystickY = 0;
// unsigned long joystickYMs = 0;
//int joystickYMsDel = 0;

// автомат обработки кнопки джойтсика
byte joystickBut = 0;
#define JOYPITCHBAND 1
#define JOYSHIFTPITCH 2
#define JOYANALOGCC 3
#define JOYDIGITALCC 4
byte joyMode = JOYPITCHBAND; // флаг  режима джойстика 
unsigned long joystickButMs = 0; // счетчик времени итерации автомата
unsigned long joystickButFixTime = 0; //счетчик времени зажатой кнопки

//
////////////////////////////////////////////////////////////////////////////////////////////////

// значения отправляемые по миди ( диапазон 0...127)
short joyXTopCcVal = 0;      
short joyXTopCcPrev = 0; // предыдущее значение для сравнения
short joyXBottomCcVal = 0;
short joyXBottomCcPrev = 0;
short joyYLeftCcVal = 0;
short joyYLeftCcPrev = 0;
short joyYRightCcVal = 0;
short joyYRightCcPrev = 0;
short joyXBend = 0;
short joyXBendPrev = 0;


//......... НОМЕРА СС-шек В МИДИ ПАКЕТЕ .............\\

// уже заняты 12-15 кнопки энкодеров, 20-29 джойстик, 36-51 кнопки, 57 шифт, 60-64 фейдеры, 102-106 энк, 
// ЦЦ вверх вниз ( X ) в режиме АНАЛОГ
byte joyXAnlgTopCc = 20;
byte joyXAnlgBottomCc = 21;
// ЦЦ вверх вниз в режиме ЦИФРА
byte joyXTopCc = 22;
byte joyXBottomCc = 23;
// ЦЦ влево-вправо ( Y ) в режиме ПИТЧ ( по иксу питче аналог, по игреку (лево право) аналог2)
byte joyYAnlg2LeftCc = 24;
byte joyYAnlg2RightCc = 25;
// ЦЦ влево-вправо в режиме АНАЛОГ ( по иксу аналог1, по игреку1)
byte joyYAnlgLeftCc = 26; 
byte joyYAnlgRightCc = 27; 
// ЦЦ влево-вправо в режиме ЦИФРА
byte joyYLeftCc = 28; 
byte joyYRightCc = 29; 








//начальные значения среднего положения, корректируются при включении (в конце setup()  )
short centerX = 512;
short centerY = 512;

// Время последнего нажатия на кнопку джойстика
unsigned long msJoyButtonPushTime;
// Время длинного нажатия на кнопку джойстика
const unsigned int MS_JOY_LONG_PRESS_DELAY = 700;

// Время последнего действия 
//это новые переменные для функции ожидания видимо. тупо мигать если простой
unsigned long msLastActionTime;
const unsigned short SEC_IDLE_TIME = 60000; // время простоя для вкл деморежима в милисекундах
const unsigned short MS_IDLE_COLOR_DELAY = 1000;



//................ПРОЦЕДУРЫ ............................







void get_center_joystick(){
//// процедура установки джойстика в ноль 
  int sumCenterX = 0; // нулевое положение по X и Y 
  int sumCenterY = 0;
  
  
  for(byte i=0; i<200; i++){ // наработка начальной базы для фильтра аналоговых значений
    joyXanalog.update();
    joyYanalog.update();
    analogWrite(JOYSTICK_BLUE_PIN, i);
    if(i > 100) analogWrite(JOYSTICK_GREEN_PIN, i-100);
    delay(1);
  }
  for(int i=1; i<11; i++){ // выбор среднего из 10
    joyXanalog.update();
    //Serial.print("joyXanalog.getValue()=");
    //Serial.print(joyXanalog.getValue());
    sumCenterX += joyXanalog.getValue();
    
    joyYanalog.update();
    sumCenterY += joyYanalog.getValue();
    analogWrite(JOYSTICK_BLUE_PIN, 200/i);
    if (i > 5) analogWrite(JOYSTICK_GREEN_PIN, 100/i);
    delay(50);
  }
  analogWrite(JOYSTICK_BLUE_PIN, 0);
  analogWrite(JOYSTICK_GREEN_PIN, 0);
  //Serial.print(" sumCenterX=");
  //Serial.println(sumCenterX);

  centerX = sumCenterX/10;
  centerY = sumCenterY/10;
  #ifdef DEBUGJOY
    Serial.print(" Calibrated CenterX=");
  Serial.print(centerX);
  Serial.print("Calibrated  CenterY=");
  Serial.println(centerY);
  #endif
}// get_center_joystick()



void dropActiveNotes() {
  for (byte i = 0; i < COLS; i++) {
    for (byte j = 0; j < ROWS; j++) {
      // Если пэд, и он нажат
      if (j > 0 && i > 2 && keypadState[j][i] == LOW) {
        byte note = kpdNote[i - 3][ROWS - j - 1] + bank * KBD_COUNT + noteOffset; // KBD_COUNT = 16
        noteOff(midiChannel, note, 127);
        keypadState[j][i] == HIGH;
      }
    }
  }
}//dropActiveNotes()



/**
 * Поставить цвет банка
 */ 
 // отличается от первой версии выводом цвета на светик
void bankColorOn(int bank) {
  int index = bank;
  if (index < 0) {
    index = -index;
  }
  digitalWrite(RED_PIN, bankColors[index][0]);
  digitalWrite(GREEN_PIN, bankColors[index][1]);
  digitalWrite(BLUE_PIN, bankColors[index][2]);

  ///analogWrite(JOYSTICK_RED_PIN, bankColors[index][0] == HIGH ? 200 : 0);
  //analogWrite(JOYSTICK_GREEN_PIN, bankColors[index][1] == HIGH ? 200 : 0);
  //analogWrite(JOYSTICK_BLUE_PIN, bankColors[index][2] == HIGH ? 200 : 0);
  
  analogWrite(JOYSTICK_RED_PIN, 0);
  analogWrite(JOYSTICK_GREEN_PIN, 0);
  analogWrite(JOYSTICK_BLUE_PIN, 200);
}



void printMatrixIfChanged() {
  bool matrixEq = true;
  for (byte i = 0; i < ROWS; i++) {
    for (byte j = 0; j < COLS; j++) {
      if (keypadState[i][j] != keypadStatePrev[i][j]) {
        matrixEq = false;
        break;
      }
    }
    if (!matrixEq) {
      break;
    }
  }
  if (!matrixEq) {
    Serial.println("");
    for (byte i = 0; i < ROWS; i++) {
      for (byte j = 0; j < COLS; j++) {
        Serial.print("    ");
        Serial.print(keypadState[i][j]);
        keypadStatePrev[i][j] = keypadState[i][j];
      }
      Serial.println("");
    }
  }
}



/**
 * Деморежим при простое
 * светодиоды на джойстике тупо мигают RGB
 */
void demoModeOnIdle(unsigned long msCurrentTime) {
  // Прошло ли достаточно времени для включения деморежима?
  if ((msCurrentTime - msLastActionTime) < SEC_IDLE_TIME) {
    return;
  }

  // Деморежим
  // ?
  //dropActiveNotes();
  static unsigned long msNextColor;
  if (msCurrentTime > msNextColor) {
    msNextColor = msCurrentTime + MS_IDLE_COLOR_DELAY;
    // Ставим новый цвет
              /* было так
              analogWrite(JOYSTICK_RED_PIN, random(0, 200));
              analogWrite(JOYSTICK_GREEN_PIN, random(0, 200));
              analogWrite(JOYSTICK_BLUE_PIN, random(0, 200));
              */
  //стало так
  //вероятность появления каждого цвета = 50%
  if (random(2)) analogWrite(JOYSTICK_RED_PIN, 0);
  else analogWrite(JOYSTICK_RED_PIN, random(1, 20));
  if (random(2)) analogWrite(JOYSTICK_GREEN_PIN, 0);
  else analogWrite(JOYSTICK_GREEN_PIN, random(1, 20));
  if (random(2)) analogWrite(JOYSTICK_BLUE_PIN, 0);
  else analogWrite(JOYSTICK_BLUE_PIN, random(1, 20));
  }
}

// печатаем в сериал сколько петель за секунду прошло
void printLoopsPerSecond(unsigned long loopTime) {
  static unsigned int loopsPerSecond = loopTime;
  static unsigned long lastSecondTime = 0;
  static unsigned int second = 0;

  loopsPerSecond++;

  if (loopTime >= lastSecondTime + 1000) {
    Serial.print("loopsPerSecond("); Serial.print(second); Serial.print(")="); Serial.println(loopsPerSecond);
    lastSecondTime = loopTime;
    loopsPerSecond = 0;
    second++;
  }
}


void selectMidiChan() {
  // Матрица
  for (byte i = 0; i < COLS; i++) {
    // Обнуляем i-й бит
    byte col = 0b11111111 ^ (1 << i);

    // Записываем значение в сдвиговый регистр
    digitalWrite(SHIFT_LATCH_PIN, LOW);
     shiftOut(SHIFT_DATA_PIN, SHIFT_CLOCK_PIN, MSBFIRST, col);  
  digitalWrite(SHIFT_LATCH_PIN, HIGH); //устанавливаем HIGH на latchPin, чтобы проинформировать регистр, что передача окончена.
  //delay(1);  // ЗАКОММЕНТИРОВАТЬ ПОСЛЕ ТЕСТОВ



    // Узнаём активные столбцы матрицы
    for (byte j = 0; j < ROWS; j++) {
      dval = digitalRead(rowPins[j]);

      // Кнопка нажата
      if (dval == LOW) {
        if (j > 0 && i > 2) {
          midiChannel = 4 - j + (COLS - i - 1) * 4;
        }
      }
    }
  }
}


//// Миди-сообщения

// First parameter is the event type (0x09 = note on, 0x08 = note off).
// Second parameter is note-on/note-off, combined with the channel.
// Channel can be anything between 0-15. Typically reported to the user as 1-16.
// Third parameter is the note number (48 = middle C).
// Fourth parameter is the velocity (64 = normal, 127 = fastest).
void noteOn(byte channel, byte pitch, byte velocity) {
  // MIDI.h
  //MIDI.sendNoteOn(pitch, velocity, channel);
  // MIDIUSB.h
  midiEventPacket_t noteOn = {0x09, 0x90 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOn);
  MidiUSB.flush();

  msLastActionTime = millis();

  Serial.print("Note #"); Serial.print(pitch); Serial.print(" => "); Serial.println(velocity);
}

void noteOff(byte channel, byte pitch, byte velocity) {
  // MIDI.h
  //MIDI.sendNoteOff(pitch, velocity, channel);
  // MIDIUSB.h
  midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOff);
  MidiUSB.flush();

  msLastActionTime = millis();

  Serial.print("Note Off #"); Serial.print(pitch); Serial.print(" => ");  Serial.println(velocity);
}

void controlChange(byte channel, byte control, byte value) {
  // MIDI.h
  //MIDI.sendControlChange(control, value, channel);
  // MIDIUSB.h
  midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
  MidiUSB.sendMIDI(event);
  MidiUSB.flush();

  msLastActionTime = millis();
  
  Serial.print("CC #"); Serial.print(control); Serial.print(" => ");  Serial.println(value);
}

void programChange(byte channel, byte program) {
  // MIDI.h
  //MIDI.sendProgramChange(program, channel);
  // MIDIUSB.h
  midiEventPacket_t event = {0x0C, 0xC0 | channel, program};
  MidiUSB.sendMIDI(event);
  MidiUSB.flush();

  msLastActionTime = millis();

  //Serial.print("PC #"); Serial.println(program);
}

void pitchBend(byte channel, short bend) {
  // MIDI.h
  //MIDI.sendProgramChange(program, channel);
  // MIDIUSB.h
  //bend += 0x2000;
  midiEventPacket_t event = {0x0E, 0xE0 | channel, bend & 0b1111111, (bend >> 7) & 0b1111111};
  MidiUSB.sendMIDI(event);
  MidiUSB.flush();

  msLastActionTime = millis();

  //Serial.print("PB #"); Serial.println(bend);
}


void setup()
{
  //MIDI.begin(1);  //Инициализация MIDI интерфейса
  Serial.begin(9600);
   
  //ShiftOut
  pinMode(SHIFT_CLOCK_PIN, OUTPUT);
  pinMode(SHIFT_DATA_PIN, OUTPUT);
  pinMode(SHIFT_LATCH_PIN, OUTPUT);
  digitalWrite(SHIFT_LATCH_PIN, HIGH); // правильно его оставить в HIGH

  // Ряды клавиатуры - входы
  for (byte j = 0; j < ROWS; j++) {
    pinMode(rowPins[j], INPUT_PULLUP);
  }

  // Аналоговые входы
  for (byte i = 0; i < ANALOGS; i++) {
    pinMode(analogPins[i], INPUT);
  }
  //analogReference(DEFAULT);
  //analogReference(EXTERNAL);




  // RGB-светодиод
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);


  // Дальномер и джойстик
  pinMode(irPin, INPUT);
  pinMode(joyXPin, INPUT);
  pinMode(joyYPin, INPUT);
  pinMode(joyButtonPin, INPUT_PULLUP);

  Serial.begin(15200);
  Serial.println("reset");
  delay(10);
  
  selectMidiChan();         // раскомментировать после отладки
  bankColorOn(bank);
  get_center_joystick();//// процедура установки джойстика в ноль 
    
}//setup()






void loop(){
  
 /*/// отладка состояний автомата джойстика
  if(joystickXPrevious != joystickX){
    Serial.print("auto joystickX = ");
    Serial.println(joystickX);
    joystickXPrevious = joystickX;
  }
  */
  unsigned long start = ms = millis();
  static uint8_t last_input_states = 0;

  
  //новый кусок кода
  demoModeOnIdle(start);// РАСКОММЕНТИРОВАТЬ после тестов
	
  // Матрица
  // для колонок от 1 до 7 ( по правильному для строк имеется ввиду) i - строка
  for (byte i = 0; i < COLS; i++) { 	// ver 1.0
    // Обнуляем i-й бит
    byte col = 0b11111111 ^ (1 << i);
	
    // Записываем значение в сдвиговый регистр, чтобы послушать кнопки и энкодеры
    digitalWrite(SHIFT_LATCH_PIN, LOW);
   	shiftOut(SHIFT_DATA_PIN, SHIFT_CLOCK_PIN, MSBFIRST, col);  
		//устанавливаем HIGH на latchPin, чтобы проинформировать регистр, что передача окончена.
	digitalWrite(SHIFT_LATCH_PIN, HIGH);
	
	
     //ОБРАБОТКА КЛАВИАТУРЫ
	// Узнаём активные столбцы матрицы
    for (byte j = 0; j < ROWS; j++) { //просматриваем каждую колоноку в строке ( j - колонки)
    
	switch(button[j][i]){
		
	} //switch(button[j][i]){
  
	dval = digitalRead(rowPins[j]);

      // Если кнопка [i, j]  нажата
      if (dval == LOW && keypadState[j][i] == HIGH) {
		  // ЗАКОММЕНТИРОВАТЬ строку ниже
      Serial.print("i="); Serial.print(i); Serial.print("    j="); Serial.println(j); 
        if (j > 0 && i > 2) { // для строк (3-7) т.е. кнопок 
          // Пэд нажат
          if (isShift) { // если включен режим шифт сдвига, то нажатием выбираем сс 
            byte cc = kpdCc[i - 3][ROWS - j - 1]; // номер сс в зависимости от i, j
            controlChange(midiChannel, cc, 127); // отправляет выбранный сс
          } else { // иначе нажатие пэда выбирает ноту
            byte note = kpdNote[i - 3][ROWS - j - 1] + bank * KBD_COUNT + noteOffset; // KBD_COUNT = 16; 
            noteOn(midiChannel, note, 127);
            
            // В двойном режиме посылаем вместе с нотой и CC
            if (isDouble) {
              byte cc = kpdDoubleCc[i - 3][ROWS - j - 1];
              controlChange(midiChannel, cc, 127);
            }
          }
          // Цвет пэдов
          bankColorOn(4); // красный вроде 
        } else if (j > 0 && i == 0) { // для первой строки, т.е. кнопки энкодеров и по всем столбцам
          // Кнопка банка нажата ( кнопка энкодеров)
          // Сбрасываем активные ноты
          // Переключаем банк
          if (!isShift) { 
            // Новый банк
            dropActiveNotes();
            bank = ROWS - j - 1;
            noteOffset = NOTE_OFFSET_NONE; // =0 
            controlChange(midiChannel, bankChangeCc[bank], 127);
            bankColorOn(bank);
          } else {
            if (j == 4) {
              dropActiveNotes();
              bank = -2;
              noteOffset = NOTE_OFFSET_GHOST_BANK; // = -4
              bankColorOn(bank);
            } else if (j == 3) {
              dropActiveNotes();
              bank = -1;
              noteOffset = NOTE_OFFSET_NONE; // = 0
              bankColorOn(bank);
            }
          }
        } else if (j == 0 && i == 0) {
          // Кнопка сдвига нажата
          isShift = true;
          // Цвет режима
          bankColorOn(irActive ? 6 : 5);

          // Сбрасываем активные ноты
          //dropActiveNotes();

          // Если дальномер работает, отключаем его
          // Потестировать и, если надо, закомментировать (отключить)
          irActive = false;
          controlChange(midiChannel, irCc, 127);
          irPrevPos = 127;
          // Комментировать до сих пор
        }
      } //если кнопка [i, j] нажата 
	  
	  ///////////////////////////////////////////////////////////////////////////////////
      
	  // если Кнопка уже отжата
      if (dval == HIGH && keypadState[j][i] == LOW) {
        if (j > 0 && i > 2) {
          // Пэд отпущен
          if (isShift) {
            byte cc = kpdCc[i - 3][ROWS - j - 1];
            controlChange(midiChannel, cc, 0);
          } else {
            byte note = kpdNote[i - 3][ROWS - j - 1] + bank * KBD_COUNT + noteOffset; // KBD_COUNT =16
            noteOff(midiChannel, note, 127);

            // В двойном режиме посылаем вместе с нотой и CC
            if (isDouble) {
              byte cc = kpdDoubleCc[i - 3][ROWS - j - 1];
              controlChange(midiChannel, cc, 0);
            }
          }

          // Возвращаем цвет банка
          bankColorOn(isShift ? (irActive ? 6 : 5) : bank);
        } else if (j > 0 && i == 0) {
          // Кнопка банка отжата
          // Специальные режимы по сдвигу
          // TODO
          // ...
          // ...
          if (isShift) {
            // Специальные режимы
            // Дальномер
            // Двойной режим
            if (j == 1) {
              // Нажато включение/выключение инфракрасного датчика
              irActive = !irActive;
            } else if (j == 2) {
              // Нажато включение/выключение двойного режима
              isDouble = !isDouble;
            }
          }
        } else if (j == 0 && i == 0) {
          // Кнопка сдвига отжата
          isShift = false;
          bankColorOn(bank);
        }
      } // кнопка отжата


      // ОПРОС ЭНКОДЕРОВ
      // j == 0 - энкодер на Шифте
      if (i == 4 && j > 0) { // Ряд с энкодерами
        currentTime[j] = millis();
        //if ((currentTime[j] - loopTime[j]) > 1) { // проверяем каждые 2мс (500 Гц)
        //тестовый   опрос чуть быстрее/ вроде как отзывчевее стало.
        if ((currentTime[j] - loopTime[j]) > 0) { // проверяем каждые 1мс (1000 Гц)
        encoderA[j] = keypadState[j][2];
          encoderB[j] = keypadState[j][1];
      
          if (!encoderA[j] && encoderAprev[j]) {    // если состояние изменилось с положительного к нулю
            byte step;
            if ((currentTime[j] - msEncoderChangeTime[j]) > msEncoderSlowDelay) {
              // Медленно
              step = encoderSlowStep;
            } else if ((currentTime[j] - msEncoderChangeTime[j]) > msEncoderMidDelay) {
              // Средне
              step = encoderMidStep;
            } else {
              // Быстро
              step = encoderStep;
            }

            if (encoderB[j]) {
              encoderPos[j] += step;               
            } else {
              encoderPos[j] -= step;              
            }
          }
          encoderAprev[j] = encoderA[j];     // сохраняем значение А для следующего цикла 
      
          if (encoderPos[j] < 0) {
            encoderPos[j] = 0;
          } else if (encoderPos[j] > 127) {
            encoderPos[j] = 127;
          }
      
          if (encoderPos[j] != encoderPrevPos[j]) {
            controlChange(midiChannel, encoderCc[j], encoderPos[j]);
            // ЗАКОММЕНТИРОВАТЬ отладку в серийник
			Serial.print("CC #");  Serial.print(encoderCc[j]); Serial.print(" => "); Serial.println(encoderPos[j]);
            encoderPrevPos[j] = encoderPos[j];
            msEncoderChangeTime[j] = currentTime[j];
          }
          
          loopTime[j] = currentTime[j];
        }
      }

      
      keypadState[j][i] = dval;
    }
  }  // for i =   	опрос матрицы конец 


  // Отдельный энкодер (на Сдвиге)
/*
  byte j = 0;
  currentTime[j] = millis();
  if ((currentTime[j] - loopTime[j]) > 1) { // проверяем каждые 2мс (500 Гц)
    encoderA[j] = keypadState[j][2];
    encoderB[j] = keypadState[j][1];

    if (!encoderA[j] && encoderAprev[j]) {    // если состояние изменилось с положительного к нулю
      byte step;
      if ((currentTime[j] - msEncoderChangeTime[j]) > msEncoderSlowDelay) {
        // Медленно
        step = encoderSlowStep;
      } else if ((currentTime[j] - msEncoderChangeTime[j]) > msEncoderMidDelay) {
        // Средне
        step = encoderMidStep;
      } else {
        // Быстро
        step = encoderStep;
      }

      if (encoderB[j]) {
        encoderPos[j] += step;               
      } else {
        encoderPos[j] -= step;              
      }
    }
    encoderAprev[j] = encoderA[j];     // сохраняем значение А для следующего цикла 

    if (encoderPos[j] < 0) {
      encoderPos[j] = 0;
    } else if (encoderPos[j] > 127) {
      encoderPos[j] = 127;
    }

    if (encoderPos[j] != encoderPrevPos[j]) {
      controlChange(midiChannel, encoderCc[j], encoderPos[j]);
      //Serial.print("CC #");  Serial.print(encoderCc[j]); Serial.print(" => "); Serial.println(encoderPos[j]);
      encoderPrevPos[j] = encoderPos[j];
      msEncoderChangeTime[j] = currentTime[j];
    }
    
    loopTime[j] = currentTime[j];
  }
  /**/


  // Аналоговые входы
  for (byte i = 0; i < ANALOGS; i++) {
    analogs[i].update();
    val = 127 - analogs[i].getValue() / 8;
    if (val < 3) {
      val = 0;
    } else if (127 - val < 3) {
      val = 127;
    }
    if (abs(val - analogPrevPos[i]) > 2) {
      controlChange(midiChannel, analogCc[i], val);
      analogPrevPos[i] = val;
    }
  }

	// инфракрасный дальномер
    //для отладки раскоментитьir.update(); short val_ = ir.getValue(); Serial.println(val_);
  // Дальномер
  if (irActive) {
    ir.update();
    //short val_ = ir.getValue();
    //Serial.println(val_);
    
    short val = map(ir.getValue(), IR_VALUE_MAX, IR_VALUE_MIN, 0, 127);
    if (val < 4) {
      val = 0;
    } else if (val > 125) {
      val = 127;
    }
    //Serial.println(val);
    if (abs(val - irPrevPos) > 0) {
      controlChange(midiChannel, irCc, val);
      irPrevPos = val;
    }
  }

  
  /* для просмотра проиницилизир. переменных. этот код можно удалить
byte joyMode = JOYPITCHBAND; // режим джойстика 
#define JOYPITCHBAND 1
#define JOYSHIFTPITCH 2
#define JOYANALOGCC 3
#define JOYDIGITALCC 4
  

// автомат обработки X(вертикали) джойтсика
byte joystickX = 0;
unsigned long joystickXMs = 0;
int joystickXMsDel = 0; 
short newValX=0; 
short ValX=0; 

// автомат обработки X(вертикали) джойтсика
byte joystickY = 0;
unsigned long joystickYMs = 0;
int joystickYMsDel = 0;

// автомат работы кнопки джойтсика
byte joystickBut = 0;
unsigned long joystickButMs = 0;
int joystickButDel = 0;
*/

// автомат обработчик ВЕРТИКАЛЬНОГО X перемещения ДЖОЙСТИКА
// ведомый. флаги на перекл состояний из автомата кнопки джойстика и кнопки шифт
switch(joystickX){
  case 0:
 // joystickXMs = ms;
//  joystickXMsDel = 1; // задержка в следующей итерации
  joystickX = 5; // GO
  break;
  case 5:
  // опрашиваем джой по иксу и 
  //выберем в каком режиме ему работать
  joyXanalog.update();
  valX = joyXanalog.getValue();
  if(joyMode == JOYPITCHBAND){
	  joystickX = 10;// GO в работу режима питч
  } else if (joyMode == JOYSHIFTPITCH){
	  joystickX = 13;// GO в работу фикс питч
  } else if (joyMode == JOYANALOGCC){
	  joystickX = 20;// GO в работу аналогового режима режима
  } else if (joyMode == JOYDIGITALCC){
	joystickX = 30;// GO в работу цифрового режима
  }
  break;
  
  case 10:
	  // режим ПИТЧ-БЭНД
	  // отправляем значения ПИТЧА от 0 до 127, центральная точка = 64 ( на джойстике 512 физич)
	  // приводим данные с к формату 0...127
	  if(valX > (centerX-JOYCENTERCORR)) { 
		  joyXBend = map(valX, centerX+JOYCENTERCORR, 1023-JOYEDGECORR, 64, 127); 
		  if(joyXBend < 64) joyXBend = 64; // ограничыввем выход за рамки
		  if(joyXBend < 127) joyXBend = 127;
	
	  } else { // когда значение ниже средней позиции джойстика
		  joyXBend = map(valX, 0+JOYEDGECORR, centerX-JOYCENTERCORR, 0, 64);
	  	  if(joyXBend > 64) joyXBend = 64;
		  if(joyXBend < 0) joyXBend = 0;
	  }
	  // отправим результат питча по МИДИ, если обновилось значение
	  if ( joyXBend != joyXBendPrev) { 
		  pitchBend(midiChannel, joyXBend * 128);
		  joyXBendPrev = joyXBend;
		  #ifdef DEBUGJOY 
		  Serial.print("new pitch = "); 
		  Serial.println(joyXBend);
		  #endif
	  }// sendit it renew came
	joystickX = 5; // GO на очередной опрос
  break;
  
  case 13:
  // режим ШИФТ-ФИКС
  //основное условие проверяет, стоит ли флаг шифта = PUSHED
  // пока он стоит, крутимся внутри этого кейса
  // когда флаг изменится, выходим в case 5
  joystickX = 5; // GO
  break;
  
  case 20:
  // режим АНАЛОГ
  // отправляем аналоговые значения джойстика в формате 0-127 вверх и 0-127  вниз от середины ( от физич 512)
	// одна цецешка выше середины , приводим ее значения к 0-127
	// вторая ниже
      if(valX > centerX){
			joyXBottomCcVal= map(valX, 512-JOYCENTERCORR, 0+JOYEDGECORR , 0, 127); // корректировка нулевых мертвых зон
			if(joyXBottomCcVal > 127) joyXBottomCcVal = 127;
			else if(joyXBottomCcVal < 0) joyXBottomCcVal =0 ;
	  } else {
		  joyXTopCcVal= map(valX, 512+JOYCENTERCORR, 1023-JOYEDGECORR , 0, 127);
		  if(joyXTopCcVal > 127) joyXTopCcVal = 127;
		  else if(joyXTopCcVal < 0) joyXTopCcVal = 0;
	  }  			
	// отправка миди, если значение изменилось
	if (joyXBottomCcVal != joyXBottomCcPrev) { 
      controlChange(midiChannel, joyXAnlgBottomCc, joyXBottomCcVal);
      joyXBottomCcPrev = joyXBottomCcVal;
		  #ifdef DEBUGJOY 
		  Serial.print("anlg joy X bot = "); 
		  Serial.println(joyXBottomCcVal);
		  #endif
    }
	// отправка миди, если значение изменилось
    if (joyXTopCcVal != joyXTopCcPrev) { 
      controlChange(midiChannel, joyXAnlgTopCc, joyXTopCcVal);
      joyXTopCcPrev = joyXTopCcVal;
		  #ifdef DEBUGJOY
		  Serial.print("anlg joy X top = "); 
		  Serial.println(joyXTopCcVal);
		  #endif
    }
	joystickX = 5; // GO на очередной опрос
  break;
  
  case 30:
    // режим ЦИФРА
	// такой же как режим аналог, только значения отправляются либо ноль либо 127
	if(valX < 360){ // джойстик в нижнем положении 
		joyXBottomCcVal = 127;
		// отправка миди, если значение изменилось
		if (joyXBottomCcVal != joyXBottomCcPrev) { 
		  controlChange(midiChannel, joyXBottomCc, joyXBottomCcVal);
		  joyXBottomCcPrev = joyXBottomCcVal;
			  #ifdef DEBUGJOY 
			  Serial.print("digit joy X bot = "); 
			  Serial.println(joyXBottomCcVal);
			  #endif
		}
	} else if (valX < 680){ // джойстик по центру
		joyXTopCcVal = 0;
		joyXBottomCcVal = 0;
		// отправка миди, если значение изменилось
		if (joyXTopCcVal != joyXTopCcPrev) { 
		  controlChange(midiChannel, joyXTopCc, joyXTopCcVal);
          joyXTopCcPrev = joyXTopCcVal;
		  #ifdef DEBUGJOY
		  Serial.print("digit joy X top = "); 
		  Serial.println(joyXTopCcVal);
		  #endif
		}
		// отправка миди, если значение изменилось
		if (joyXBottomCcVal != joyXBottomCcPrev) { 
		  controlChange(midiChannel, joyXBottomCc, joyXBottomCcVal);
		  joyXBottomCcPrev = joyXBottomCcVal;
			  #ifdef DEBUGJOY 
			  Serial.print("digit joy X bot = "); 
			  Serial.println(joyXBottomCcVal);
			  #endif
		}
    } else { // джойстик в верхнем положении
		joyXTopCcVal = 127;
		// отправка миди, если значение изменилось
		if (joyXTopCcVal != joyXTopCcPrev) { 
		  controlChange(midiChannel, joyXTopCc, joyXTopCcVal);
          joyXTopCcPrev = joyXTopCcVal;
		  #ifdef DEBUGJOY
		  Serial.print("digit joy X top = "); 
		  Serial.println(joyXTopCcVal);
		  #endif
		}
	}
  joystickX = 5; // GO на очередной опрос
  break;

}//switch joystickX
  
  
  
  
 // автомат обработчик горизонтального Y перемещения ДЖОЙСТИКА
 // ведомый. флаги на перекл состояний из автомата кнопки джойстика и кнопки шифт
 switch(joystickY){
  case 0:
 // joystickYMs = ms;
 //joystickYMsDel = 1; // задержка в следующей итерации
  joystickY = 5; // GO
  break;
  case 5:
  // опрашиваем джой по иксу и 
  //выберем в каком режиме ему работать
  joyYanalog.update();
  valY = joyYanalog.getValue();
  if(joyMode == JOYPITCHBAND){
	  joystickY = 10;// GO в работу режима питч
  } else if (joyMode == JOYSHIFTPITCH){
	  joystickY = 10;// GO в работу фикс питч
  } else if (joyMode == JOYANALOGCC){
	  joystickY = 10;// GO в работу аналогового режима режима
  } else if (joyMode == JOYDIGITALCC){
	joystickY = 30;// GO в работу цифрового режима
  }
  break;
  
  case 10:
	  // режим ПИТЧ-БЭНД и режим АНАЛОГ
	 // отправляем аналоговые значения джойстика в формате 0-127 влево  и 0-127  вправо от середины ( от физич 512)
	// отправляем на одну из двух СС в зависимости от выбранного режима
	//	одна цецешка опрашивает джойстик выше середины ,  вторая ниже 
	// приводим значения к 0-127 
      if(valY > centerX){
			joyYLeftCcVal= map(valY, 512-JOYCENTERCORR, 0+JOYEDGECORR , 0, 127); // корректировка нулевых мертвых зон
			if(joyYLeftCcVal > 127) joyYLeftCcVal = 127;
			else if(joyYLeftCcVal < 0) joyYLeftCcVal =0 ;
	  } else {
		  joyYRightCcVal= map(valY, 512+JOYCENTERCORR, 1023-JOYEDGECORR , 0, 127);
		  if(joyYRightCcVal > 127) joyYRightCcVal = 127;
		  else if(joyYRightCcVal < 0) joyYRightCcVal = 0;
	  }  			
	// отправка миди, если значение изменилось
	if (joyYLeftCcVal != joyYLeftCcPrev) {
		joyYLeftCcPrev = joyYLeftCcVal;
		if((joyMode == JOYPITCHBAND) || (joyMode == JOYSHIFTPITCH)) { // если режим питча то отправляем на СС Anlg2 иначе на Anlg 
			controlChange(midiChannel, joyYAnlg2LeftCc, joyYLeftCcVal);
		} else if(joyMode == JOYANALOGCC) {
			controlChange(midiChannel, joyYAnlgLeftCc, joyYLeftCcVal);
		}
      
		  #ifdef DEBUGJOY 
		  Serial.print("anlg joy Y Left= " ); 
		  Serial.println(joyYLeftCcVal);
		  #endif
    }//midi send
	// отправка миди, если значение изменилось
    if (joyYRightCcVal != joyYRightCcPrev) { 
		joyYRightCcPrev = joyYRightCcVal;
		if((joyMode == JOYPITCHBAND) || (joyMode == JOYSHIFTPITCH)) { // если режим питча или фикс питча то отправляем на СС Anlg2 иначе на Anlg 
			controlChange(midiChannel, joyYAnlg2RightCc, joyYRightCcVal);
		} else if(joyMode == JOYANALOGCC) {
			controlChange(midiChannel, joyYAnlgRightCc, joyYRightCcVal);
		}
		#ifdef DEBUGJOY
        Serial.print("anlg joy Y Right = "); 
	    Serial.println(joyYRightCcVal);
		#endif
    }//midi send
	joystickY = 5; // GO на очередной опрос
  break;
 
  case 30:
    // режим ЦИФРА
	// такой же как режим аналог, только значения отправляются либо ноль либо 127
	if(valY < 360){ // джойстик в нижнем положении 
		joyYLeftCcVal = 127;
		// отправка миди, если значение изменилось
		if (joyYLeftCcVal != joyYLeftCcPrev) { 
		  controlChange(midiChannel, joyYLeftCc, joyYLeftCcVal);
		  joyYLeftCcPrev = joyYLeftCcVal;
			  #ifdef DEBUGJOY 
			  Serial.print("digit joy Y Left = "); 
			  Serial.println(joyYLeftCcVal);
			  #endif
		}
	} else if (valY < 680){ // джойстик по центру
		joyYRightCcVal = 0;
		joyYLeftCcVal = 0;
		// отправка миди, если значение изменилось
		if (joyYRightCcVal != joyYRightCcPrev) { 
		  controlChange(midiChannel, joyYRightCc, joyYRightCcVal);
          joyYRightCcPrev = joyYRightCcVal;
		  #ifdef DEBUGJOY
		  Serial.print("digit joy Y Right = "); 
		  Serial.println(joyYRightCcVal);
		  #endif
		}
		// отправка миди, если значение изменилось
		if (joyYLeftCcVal != joyYLeftCcPrev) { 
		  controlChange(midiChannel, joyYLeftCc, joyYLeftCcVal);
		  joyYLeftCcPrev = joyYLeftCcVal;
			  #ifdef DEBUGJOY 
			  Serial.print("digit joy Y Left = "); 
			  Serial.println(joyYLeftCcVal);
			  #endif
		}
    } else { // джойстик в верхнем положении
		joyYRightCcVal = 127;
		// отправка миди, если значение изменилось
		if (joyYRightCcVal != joyYRightCcPrev) { 
		  controlChange(midiChannel, joyYRightCc, joyYRightCcVal);
          joyYRightCcPrev = joyYRightCcVal;
		  #ifdef DEBUGJOY
		  Serial.print("digit joy Y Right = "); 
		  Serial.println(joyYRightCcVal);
		  #endif
		}
	}
	joystickY = 5; // GO на очередной опрос
  break;
 	
} // switch(joystickY) 
 
  
  
  
//// Джойстик X ..старая реализация.
  /* joyXanalog.update();
  //short val = joyXanalog.getValue();
  //Serial.println(val);
  // 507 центр
  // short centerX = 508; перенесли в сетап на коррекцию

  //Serial.println(newVal);

  if (joyMode == JOY_MODE_PITCH) {
    // Режим джойстика: питчбенд
    short newVal = val > centerX ?
      map(val, centerX, 1023, 64, 127) :
      map(val, 0, centerX, 0, 64);
  
    if (newVal < 4) {
      newVal = 0;
    } else if (abs(newVal - 64) < 4) { // плюс минус 3 от центра джойстика урезаем
      newVal = 64;
    } else if (127 - newVal < 4) {// плюс минус 3 от центра джойстика урезаем
      newVal = 127;
    }
    if (abs(newVal - joyXBendPrev) > 0) { // если обновилось значение, отправим результат
      pitchBend(midiChannel, newVal * 128);
      joyXBendPrev = newVal;
      //Serial.print("Joy X val: "); Serial.println(newVal);
    }
	
  } else {
    // Режим джойстика: эффектор
    short newVal = val > centerX ?
      map(val, centerX, 1023, 0, 127) :
      map(val, 0, centerX, -127, 0);
  
    if (abs(newVal) < 5) {
      newVal = 0;
    } else if (newVal + 127 < 4) { 
      newVal = -127;
    } else if (127 - newVal < 4) {
      newVal = 127;
    }

    if (newVal < 0) {
      joyXBottomCcVal = -newVal;
      joyXTopCcVal = 0;
    } else if (newVal > 0) {
      joyXBottomCcVal = 0;
      joyXTopCcVal = newVal;
    } else {
      joyXBottomCcVal = 0;
      joyXTopCcVal = 0;
    }
  
    if (joyXBottomCcVal != joyXBottomCcPrev) {
      controlChange(midiChannel, joyXBottomCc, joyXBottomCcVal);
      joyXBottomCcPrev = joyXBottomCcVal;
    }
    if (joyXTopCcVal != joyXTopCcPrev) {
      controlChange(midiChannel, joyXTopCc, joyXTopCcVal);
      joyXTopCcPrev = joyXTopCcVal;
    }
  } // else ( if  not PITCH )

   
  


  // джойстик Y старая реализация
  
  //joyYfilter = 0.9 * joyYfilter + analogRead(joyYPin);
  //joyYfilter = joyYfilter - (joyYfilter >> 4) + analogRead(joyYPin);
  //joyYfilter = (1 - 0.9) * analogRead(joyYPin) / 8 + 0.9 * joyYfilter;
  //Serial.println(joyYfilter);

  joyYanalog.update();
  val = joyYanalog.getValue();
  //Serial.println(val);

  // 517 центр
  //centerY = 522;
  short newVal = val > centerY ?
    map(val, centerY, 1023, 0, -127) :
    map(val, 0, centerY, 127, 0);

  if (abs(newVal) < 11) {
    newVal = 0;
  } else if (newVal + 127 < 4) {
    newVal = -127;
  } else if (127 - newVal < 4) {
    newVal = 127;
  } 
 
  if (newVal < 0) {
    joyYLeftCcVal = -newVal;
    joyYRightCcVal = 0;
  } else if (newVal > 0) {
    joyYLeftCcVal = 0;
    joyYRightCcVal = newVal;
  } else {
    joyYLeftCcVal = 0;
    joyYRightCcVal = 0;
  }

  // В режиме эффектора плавные ЦЦ заменяются кнопочными
  if (joyMode == JOY_MODE_EFFECTOR) {
    joyYLeftCcVal = joyYLeftCcVal > 64 ? 127 : 0;
    joyYRightCcVal = joyYRightCcVal > 64 ? 127 : 0;
  }

  if (joyYLeftCcVal != joyYLeftCcPrev) {
    controlChange(midiChannel, joyMode == JOY_MODE_PITCH ? joyYLeft1Cc : joyYLeft2Cc, joyYLeftCcVal);
    joyYLeftCcPrev = joyYLeftCcVal;
  }
  if (joyYRightCcVal != joyYRightCcPrev) {
    controlChange(midiChannel, joyMode == JOY_MODE_PITCH ? joyYRight1Cc : joyYRight2Cc, joyYRightCcVal);
    joyYRightCcPrev = joyYRightCcVal;
  }
  

  
  
  /// Кнопка на джойстике ..старая реализация
  dval = digitalRead(joyButtonPin);
  if (dval == LOW && joyButtonPrev == HIGH) {
    //controlChange(midiChannel, joyButtonCc, 127);
    // Кнопка на джойстике нажата
	analogWrite(JOYSTICK_RED_PIN, 255);
	analogWrite(JOYSTICK_GREEN_PIN, 255);
	analogWrite(JOYSTICK_BLUE_PIN, 255);
    joyButtonPrev = dval;
    msJoyButtonPushTime = start; //!! ПО НАЖАТИИ НА КНОПКУ НАЧИНАЕМ СЧИТАТЬ
    //Serial.println("joyBut"); // временно для теста нажатие кнопки на джойстике
  } else if (dval == HIGH && joyButtonPrev == LOW) {
    // Кнопка на джойстике отжата
	analogWrite(JOYSTICK_RED_PIN, 0);
	analogWrite(JOYSTICK_GREEN_PIN, 0);
	analogWrite(JOYSTICK_BLUE_PIN, 0);
    
    if ((start - msJoyButtonPushTime) < MS_JOY_LONG_PRESS_DELAY) {
      // Если нажатие короткое, посылаем ЦЦ
      controlChange(midiChannel, joyButtonCc, 127);
      //Serial.print("Joy CC: 127 #"); Serial.println(joyButtonCc);
    } else {
      // Если нажатие длинное, переключаем режим
      joyMode = joyMode == JOY_MODE_PITCH ? JOY_MODE_EFFECTOR : JOY_MODE_PITCH;
      // Сбрасываем питчбенд и привязанные к джойстику ЦЦ
      {
        if (joyXBendPrev != 64) {
          pitchBend(midiChannel, PITCH_BEND_CENTER);
          joyXBendPrev = 64;
        }
        if (joyXTopCcPrev != 0) {
          controlChange(midiChannel, joyXTopCc, 0);
          joyXTopCcPrev = 0;
        }
        if (joyXBottomCcPrev != 0) {
          controlChange(midiChannel, joyXBottomCc, 0);
          joyXBottomCcPrev = 0;
        }
        if (joyYLeftCcPrev != 0) {
          controlChange(midiChannel, joyYLeftCc, 0);
          controlChange(midiChannel, joyYLeftAltCc, 0);
          joyYLeftCcPrev = 0;
        }
        if (joyYRightCcPrev != 0) {
          controlChange(midiChannel, joyYRightCc, 0);
          controlChange(midiChannel, joyYRightAltCc, 0);
          joyYRightCcPrev = 0;
        }
      }
      //Serial.print("Joy Mode: "); Serial.println(joyMode);
    }
    joyButtonPrev = dval;
  }
//  кнопка на джойстике старая*/

  //////// Отладка
  // Матрица состояний
  //printMatrixIfChanged();  // закоментить после отладки
   //printLoopsPerSecond(start);
  //unsigned long diff = millis() - start;
  //Serial.println(diff);
// автомат обработки кнопки джойтсика

switch (joystickBut){
	case 0:
	joystickButMs = ms;
	joystickButFixTime = 2;
	joystickBut = 5; 
	#ifdef DEBUGJOY
	Serial.print("joyMode = ");
  Serial.println(joyMode);
  #endif
	break;
	
	case 5:
	// новый опрос отпущенной кнопки
	if((ms - joystickButMs) > 2){
		joystickButMs = ms;
		if(!digitalRead(joyButtonPin)){ // нажали кнопку ?
		joystickBut = 9; // GO
		}
	}//if ms
	break;
	case 9:
	// проверка, не дребезг ли
	if((ms - joystickButMs) > 20){
		joystickButMs = ms;
		if(!digitalRead(joyButtonPin)){ // зафиксировано надавливание без дребезга
			joystickBut = 14; // GO
			joystickButFixTime = ms; // начался таймер отсчета времени удержания
		} else { // зафиксирован дребезг - игнорируем 
			joystickBut = 0; // на новый опрос кнопки
		}
	}//if ms
	break;
	case 14:
	// подсчет времени удержания кнопки
	    if(digitalRead(joyButtonPin)){ // кнопку пытаются отпустить ?
			joystickBut = 17; // GO
			joystickButMs = ms;
		}
	break;
    case 17:
	// проверка, не дребезг ли отпущенной кнопки
	if((ms - joystickButMs) > 20){
		joystickButMs = ms;
		if(digitalRead(joyButtonPin)){ // зафиксировано ОТПУСКАНИЕ кнопки без дребезга
			joystickBut = 21; // GO
		}
		// иначе - если зафиксирован дребезг по отпусканию, 
		// останемся  в этом же кейсе пока  дребезг не пройдет 
	}//if ms
	break;
	case 21:
	// определение времени удержания кнопки
	if((ms - joystickButFixTime) < LONGPUSHTIME){ // кнопка нажата коротко user time
		joystickBut = 30; // GO на анализ действий по user короткому нажатию 	
	} else { //иначе кнопка нажата длительное user time
		joystickBut = 40; // GO на анализ действий по длинному user нажатию 	
	}
	break;
	case 30:
	// придпринимаем действия после КОРОТКОГО USER нажатия 
	if(joyMode == JOYPITCHBAND){ // из первого режима короткое нажатие пока что ничего
										// КОСТЫЛЬ ДЛЯ ТЕСТОВ
										analogWrite (JOYSTICK_RED_PIN, 255);
										delay(50);
										analogWrite (JOYSTICK_RED_PIN, 0);
		joystickBut = 0; // на исходную
	} else if(joyMode == JOYANALOGCC) { // если были во втором режиме 
		// вернемся в первый
		joyMode = JOYPITCHBAND;
										// КОСТЫЛЬ ДЛЯ ТЕСТОВ
										analogWrite (JOYSTICK_RED_PIN, 255);
										analogWrite (JOYSTICK_GREEN_PIN, 25);
										delay(50);
										analogWrite (JOYSTICK_RED_PIN, 0);
										analogWrite (JOYSTICK_GREEN_PIN, 0);
		joystickBut = 0; // на исходную
	} else if(joyMode == JOYDIGITALCC) { // если были в третьем режиме 
		// вернемся во второй
		joyMode = JOYANALOGCC;
										// КОСТЫЛЬ ДЛЯ ТЕСТОВ
										analogWrite (JOYSTICK_BLUE_PIN, 55);
										analogWrite (JOYSTICK_GREEN_PIN, 125);
										delay(50);
										analogWrite (JOYSTICK_BLUE_PIN, 0);
										analogWrite (JOYSTICK_GREEN_PIN, 0);
		joystickBut = 0; // на исходную
	}
	break;
	case 40:
	// придпринимаем действия после  ДЛИТЕЛЬНОГО удержания
	if(joyMode == JOYPITCHBAND){ // из первого режима длинное нажатие 
		joyMode = JOYANALOGCC; //го во второй режим
										// КОСТЫЛЬ ДЛЯ ТЕСТОВ
										analogWrite (JOYSTICK_RED_PIN, 55);
										analogWrite (JOYSTICK_GREEN_PIN, 155);
										delay(50);
										analogWrite (JOYSTICK_RED_PIN, 0);
										analogWrite (JOYSTICK_GREEN_PIN, 0);
		joystickBut = 0; // на исходную
	} else if(joyMode == JOYANALOGCC) { // если были во втором режиме 
		// уходим на третий
		joyMode = JOYDIGITALCC;
										// КОСТЫЛЬ ДЛЯ ТЕСТОВ
										analogWrite (JOYSTICK_BLUE_PIN, 155);
										analogWrite (JOYSTICK_GREEN_PIN, 25);
										delay(50);
										analogWrite (JOYSTICK_BLUE_PIN, 0);
										analogWrite (JOYSTICK_GREEN_PIN, 0);
		
	} else if(joyMode == JOYDIGITALCC) { // если были в третьем режиме 
		// ничего не делаем
										// КОСТЫЛЬ ДЛЯ ТЕСТОВ
										analogWrite (JOYSTICK_BLUE_PIN, 205);
										delay(50);
										analogWrite (JOYSTICK_BLUE_PIN, 0);
		
		joystickBut = 0; // на исходную
	}
	break;
} // switch (joystickBut)
  
} //loop








