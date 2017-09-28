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

в свиче шифта после фиксации любого нажатия (шифт короткий, долгий, даблшифт) уходим на кейс отработки логики и тут уже проверяем 
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




#define DEBUG // режим отладки влючаем.




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
const unsigned int PITCH_BEND_MIN = 0;
const unsigned int PITCH_BEND_CENTER = 0x2000;
const unsigned int PITCH_BEND_MAX = 16383;
byte joyXPin = A11; // пин оси X джойстика 
short joyXPrev = PITCH_BEND_CENTER;
short joyXBendPrev = PITCH_BEND_CENTER;
ResponsiveAnalogRead joyXanalog(joyXPin, true, 0.001);

// Джойстик вверх-вниз
byte joyXTopCc = 9;
byte joyXBottomCc = 8;

short joyXTopCcVal = 0;
short joyXBottomCcVal = 0;
short joyXTopCcPrev = 0;
short joyXBottomCcPrev = 0;

// Джойстик влево-вправо
byte joyYPin = A7; // пин оси Y джойстика 
// ЦЦ влево-вправо в режиме питчбенда
byte joyYLeftCc = 10;
byte joyYRightCc = 11;
// ЦЦ влево-вправо в режиме эффектора
byte joyYLeftAltCc = 6;
byte joyYRightAltCc = 7;

short joyYLeftCcVal = 0;
short joyYRightCcVal = 0;
short joyYLeftCcPrev = 0;
short joyYRightCcPrev = 0;

double joyYfilter = 0.0;
// Если будет сильно дребезжать влево-вправо, поставить 0.0001
ResponsiveAnalogRead joyYanalog(joyYPin, true, 0.001);


byte joyButtonPin = A0;
byte joyButtonCc = 7;
byte joyButtonPrev = HIGH;

//начальные значения среднего положения, корректируются при включении (в конце setup()  )
short centerX = 512;
short centerY = 512;

// Время последнего нажатия на кнопку джойстика
unsigned long msJoyButtonPushTime;
// Время длинного нажатия на кнопку джойстика
const unsigned int MS_JOY_LONG_PRESS_DELAY = 700;

// Режимы работы джойстика
enum {
  JOY_MODE_PITCH = 1,     // Режим питчбенда
  JOY_MODE_EFFECTOR = 2,  // Режим эффектора
};
// Режим джойстика по умолчанию: питч-бенд
byte joyMode = JOY_MODE_PITCH;

byte joystick = 0; // автомат обработки джойстика
unsigned long joystickMs = 0; // временной счетчик для автомата

// Время последнего действия 
//это новые переменные для функции ожидания видимо. тупо мигать если простой
unsigned long msLastActionTime;
const unsigned short SEC_IDLE_TIME = 60000; // время простоя для вкл деморежима в милисекундах
const unsigned short MS_IDLE_COLOR_DELAY = 1000;

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

  selectmidiChannel();         // раскомментировать после отладки


  // RGB-светодиод
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  bankColorOn(bank);

  // Дальномер и джойстик
  pinMode(irPin, INPUT);
  pinMode(joyXPin, INPUT);
  pinMode(joyYPin, INPUT);
  pinMode(joyButtonPin, INPUT_PULLUP);

  Serial.begin(15200);
  Serial.println("reset");
  delay(10);
  
  get_center_joystick();//// процедура установки джойстика в ноль 
    
}//setup()






void loop(){
  unsigned long start = millis();
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
  /**/

/*
  // Отдельный энкодер (на Сдвиге)
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

  //*
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

	// старый код инфракрасного датчик
	// Если инфракрасный датчик активен, используем его
   /*if (irActive) {
    //val = sharp.distance();
    //if (val != analogPrevPos[0]) {
    //  controlChange(1, analogCc[0], map(val, 5, 17, 0, 127));
    //  analogPrevPos[0] = val;
    //}
  
    float voltFromRaw = map(analogRead(irPin), 0, 1023, 0, 5000);
    int puntualDistance = 27.728 * pow(voltFromRaw / 1000, -1.2045);
    //if (puntualDistance >= (irTolerance * previousDistance)) {
    sampleSum += puntualDistance;
    sampleCount++;
    previousDistance = puntualDistance;
    //}
    sampleIndex++;
    if (sampleIndex >= IR_SAMPLES) {
      int accurateDistance = sampleSum / sampleCount;
      //Serial.print("accurateDistance: ");
      //Serial.println(accurateDistance);
      sampleSum = 0;
      sampleIndex = 0;
      sampleCount = 0;
      //accurateDistance = map(accurateDistance, 5, 28, 0, 127);
      accurateDistance = map(accurateDistance, 5, 19.5, 0, 127);
      if (accurateDistance < 2) {
        accurateDistance = 0;
      } else if (accurateDistance > 125) {
        accurateDistance = 127;
      }
      val = accurateDistance;
      if (val != irPrevPos) {
        controlChange(midiChannel, irCc, val);
        irPrevPos = val;
      }
    }
  }/**/
  ir.update();
    short val_ = ir.getValue();
    Serial.println(val_);
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

  //// Джойстик
  joyXanalog.update();
  short val = joyXanalog.getValue();
  //Serial.println(val);
  // 507 центр
  // short centerX = 508; перенесли в сетап на коррекцию

  //Serial.println(newVal);

  if (joyMode == JOY_MODE_PITCH) {
    // Режим джойстика: питчбенд
    short newVal = val > centerX ?
      map(val, centerX, 1023, 64, 127) :
      map(val, 0, centerX, 0, 64);
  
    if (newVal < 5) {
      newVal = 0;
    } else if (abs(newVal - 64) < 4) { // плюс минус 4 от центра джойстика урезаем
      newVal = 64;
    } else if (127 - newVal < 4) {
      newVal = 127;
    }
    if (abs(newVal - joyXBendPrev) > 0) { // если обновилось значение, отправим результат
      pitchBend(midiChannel, newVal * 128);
      joyXBendPrev = newVal;
      Serial.print("Joy X val: "); Serial.println(newVal);
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
  }

  

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
				/// код ниже  должен быть следующим кейсом свича, перед которым будет проверка в каком режиме работаем.
  
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
    controlChange(midiChannel, joyMode == JOY_MODE_PITCH ? joyYLeftCc : joyYLeftAltCc, joyYLeftCcVal);
    joyYLeftCcPrev = joyYLeftCcVal;
  }
  if (joyYRightCcVal != joyYRightCcPrev) {
    controlChange(midiChannel, joyMode == JOY_MODE_PITCH ? joyYRightCc : joyYRightAltCc, joyYRightCcVal);
    joyYRightCcPrev = joyYRightCcVal;
  }
  /**/


  //// Кнопка на джойстике
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
  /**/

  //////// Отладка
  // Матрица состояний
  //printMatrixIfChanged();  // закоментить после отладки
   //printLoopsPerSecond(start);
  //unsigned long diff = millis() - start;
  //Serial.println(diff);

  
} //loop










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
	//Serial.print(" CenterX=");
	//Serial.print(centerX);
	//Serial.print(" CenterY=");
	//Serial.println(centerY);
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


void selectmidiChannel() {
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


