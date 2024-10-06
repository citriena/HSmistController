/*********************************************************************
        Arduino Uno R3、および湿度センサHS15Pを使った挿し木床加湿制御
        HSmistController
**********************************************************************
  利用環境によって適切な加湿制御設定は異なるので、設定の最適化を行なう必要がある。
  再コンパイルしなくてもLCD KeyPad Shieldから設定を変更できるようにしている。

  変更できる設定は、
  加湿時間 (0-990秒、10秒間隔)
  休止時間（最短、および最長） (0-990秒、10秒間隔)
  加湿休止湿度、および最大加湿湿度 (50-100%)
  加湿休止VPD、および最大加湿VPD(0-9.9hPa)
  制御用測定値（湿度/VPD）
  HS15Pの補正係数 (0.01-9.99)
  温度の補正値 (-5.0-5.0℃)

  加湿時間は全体にミスト等が行き渡る時間と基本として設定する。このため、温度、湿度等
  が変わっても加湿時間は一定としている。

  一方、休止時間は温度、湿度等によって最短から最長まで変化するようにして加湿制御する
  ようにしている。現在は使っている挿し木床が小規模なことと表示の制限で、加湿および
  休止時間は990秒（16.5分）までである。たいていの場合はこれで対応できると思うが、
  大規模な施設でもっと長時間とする必要がある場合はスケッチの修正が必要である。

**********************************************************************
  必要なハードウェア
  Arduino UNO R3、およびACアダプタ
  LCD KeyPad Shield(I2C接続，もしくは従来型)
  専用シールド（自作品）
  温度センサ（サーミスタ）
  湿度センサ（HS15P、およびコンデンサ）
  パワーリレー（できればSSR（半導体リレー））
  加湿器（超音波加湿器，遠心式加湿器等）
  ケーブル、コネクタ等
**********************************************************************
  必要なソフトウェア（全てオープンソースソフトウェア）
  Arduino IDE
  SHthermistorライブラリ
  HS15Pライブラリ
  simpleKeypad_I2CもしくはsimpleKeypadライブラリ（Keypad shieldによる）
  LiquidTWI2もしくはLiquidCrystalライブラリ（Keypad shieldによる）
  DS3232RTCライブラリ（）

**********************************************************************

  初版　2015年11月
  最終改定　2022年11月26日
  緒方達志（citriena@yahoo.co.jp）

  --------------------------------------------------------------------------
  2021年3月　I2C接続のLCD KeyPad Shield対応を行った（AdafruitのI2C対応LCD keypadまたはその互換品）。
  使用するピンの設定が以下のように異なるので注意
  従来版でもI2C対応LCD keypadは使用可能だが、改訂版で非I2C LCD KeyPad Shield
  は使用できない。
  本対応により、SPIを用いる他のシールドとの併用が可能となった。
  --------------------------------------------------------------------------
  以前のシールドと、I2C接続LCD KeyPad Shieldを使って機能拡張可能な新シールドのピン使用
  なお，4番ピンと5番ピンはV1.0とV1.1で入れ替えた．以下はV1.1のピン使用

  // OLD_SHIELD (V0.1)      / NEW SHIELD for I2C LCD Keypad shield and SPI (V2.01)
  // 0: Serial
  // 1: Serial
  // 2: RH_CONT_PIN         / AUX1 (RTC interrupt input)
  // 3: ERROR_OUT           / (Key interrupt input)
  // 4: LCD                 / AUX2 (alternative)
  // 5: LCD                 / RH_CONT_PIN (Humidifier control output)
  // 6: LCD                 / AUX3 (alternative)(ERROR_OUT)
  // 7: LCD                 / HS15P_OUT
  // 8: LCD                 / HS15P_TUO
  // 9: LCD                 / THERMISTOR_EXC
  // 10: NONE               / SPI (SD card)
  // 11: THERMISTOR_EXC     / SPI (SD card)
  // 12: HS15P_OUT          / SPI (SD card)
  // 13: HS15P_TUO          / SPI (SD card)
  // A0(14): Keypad         / AUX2 (alternative)
  // A1(15): HS15P_IN       / AUX3 (alternative)
  // A2(16): THERMISTOR_ADC / HS15P_IN
  // A3(17): NONE           / THERMISTOR_ADC
  // A4(18): I2C (RTC)
  // A5(19): I2C (RTC)
  --------------------------------------------------------------------------
*********************************************************************/

//#define DEBUG
//#define SERIAL_MONITOR
#define RTC_SD
#define SdFatLite               // 標準のSDではなくSdFatライブラリを使う

//専用シールド基板のバージョンを指定する．
// V1.0以降ではI2C接続のkeypadを使ってピンを節約し、機能追加できるようにピン等の設定を変更した。
// このため，非I2C接続のLCD keypad shieldは使えない。

//#define SHIELD_VER  010 // V0.1 これはユニバーサル基板を使った試作版。今でも現場で使用中
//#define SHIELD_VER  100 // V1.0 I2Cキーパッド対応版
//#define SHIELD_VER  110 // V1.1 Ethernetシールドを併用したときに microSDカードを使えるようにした改訂版
#define SHIELD_VER  201 // V2.01 multiController共通シールド
//#define SHIELD_VER  2011 // V2.01 multiController共通シールド（特殊）


#if SHIELD_VER == 010
#define HS15P_OUT       12      // HS15Pのデジタルピン番号
#define HS15P_IN        A1      // HS15P & CapacitorのADCポート番号
#define HS15P_IN_D      15      // 上のADCポート番号をデジタルポートとして使う場合のピン番号
#define HS15P_TUO       13      // Capacitorのデジタルピン番号 
#define RH_CONT_PIN      2      // 加湿制御用出力用デジタルピン番号
#define ERROR_OUT        3      // エラー時出力ピン番号（とりあえずセンサーのみ）
#define THERMISTOR_EXC  11      // サーミスタ電圧デジタルピン（測定時だけ電圧をかけるため、Vccにはつながない（発熱抑制）
#define THERMISTOR_ADC  A2      // サーミスタ入力アナログピン； サーミスタ電圧ピンとの間に分圧抵抗、GNDとの間にサーミスタ接続
#endif

#if SHIELD_VER == 100
#define HS15P_OUT        7      // HS15Pのデジタルピン番号
#define HS15P_IN        A2      // HS15P & CapacitorのADCポート番号
#define HS15P_IN_D      16      // 上のADCポート番号をデジタルポートとして使う場合のピン番号
#define HS15P_TUO        8      // Capacitorのデジタルピン番号 
#define ERROR_OUT        6      // エラー時出力ピン番号（とりあえずセンサーのみ）
#define RH_CONT_PIN      4      // 加湿制御用出力用デジタルピン番号
#define THERMISTOR_EXC   9      // サーミスタ電圧デジタルピン（測定時だけ電圧をかけるため、Vccにはつながない（発熱抑制）
#define THERMISTOR_ADC  A3      // サーミスタ入力アナログピン；サーミスタ電圧ピンとの間に分圧抵抗、GNDとの間にサーミスタ接続
#define I2C_KEYPAD              // I2C接続のLCD keypad shield必須
#endif

#if SHIELD_VER == 110
#define HS15P_OUT        7      // HS15Pのデジタルピン番号
#define HS15P_IN        A2      // HS15P & CapacitorのADCポート番号
#define HS15P_IN_D      16      // 上のADCポート番号をデジタルポートとして使う場合のピン番号
#define HS15P_TUO        8      // Capacitorのデジタルピン番号 
#define ERROR_OUT        6      // エラー時出力ピン番号（とりあえずセンサーのみ）
#define RH_CONT_PIN      5      // 加湿制御用出力用デジタルピン番号
#define THERMISTOR_EXC   9      // サーミスタ電圧デジタルピン（測定時だけ電圧をかけるため、Vccにはつながない（発熱抑制）
#define THERMISTOR_ADC  A3      // サーミスタ入力アナログピン；サーミスタ電圧ピンとの間に分圧抵抗、GNDとの間にサーミスタ接続
#define I2C_KEYPAD              // I2C接続のLCD keypad shield必須
#endif

#if SHIELD_VER == 201
#define HS15P_OUT        8      // HS15Pのデジタルピン番号
#define HS15P_IN        A2      // HS15P & CapacitorのADCポート番号
#define HS15P_IN_D      16      // 上のADCポート番号をデジタルポートとして使う場合のピン番号
#define HS15P_TUO        9      // Capacitorのデジタルピン番号 
#define ERROR_OUT        6      // エラー時出力ピン番号（とりあえずセンサーのみ）
#define RH_CONT_PIN      5      // 加湿制御用出力用デジタルピン番号
#define THERMISTOR_EXC   2      // サーミスタ電圧デジタルピン（測定時だけ電圧をかけるため、Vccにはつながない（発熱抑制）
#define THERMISTOR_ADC  A3      // サーミスタ入力アナログピン；サーミスタ電圧ピンとの間に分圧抵抗、GNDとの間にサーミスタ接続
#define I2C_KEYPAD              // I2C接続のLCD keypad shield必須
#endif


#if SHIELD_VER == 2011
#define HS15P_OUT        8      // HS15Pのデジタルピン番号
#define HS15P_IN        A1      // HS15P & CapacitorのADCポート番号
#define HS15P_IN_D      15      // 上のADCポート番号をデジタルポートとして使う場合のピン番号
#define HS15P_TUO        9      // Capacitorのデジタルピン番号 
#define ERROR_OUT        6      // エラー時出力ピン番号（とりあえずセンサーのみ）
#define RH_CONT_PIN      5      // 加湿制御用出力用デジタルピン番号
#define THERMISTOR_EXC   2      // サーミスタ電圧デジタルピン（測定時だけ電圧をかけるため、Vccにはつながない（発熱抑制）
#define THERMISTOR_ADC  A2      // サーミスタ入力アナログピン；サーミスタ電圧ピンとの間に分圧抵抗、GNDとの間にサーミスタ接続
#define I2C_KEYPAD              // I2C接続のLCD keypad shield必須
#endif


#include <Arduino.h>
#include <EEPROM.h>
#ifdef RTC_SD
#include <SPI.h>
#ifdef SdFatLite                // 標準のSDライブラリに比べてSdFatライブラリではスケッチ容量が大幅に小さくなる。
#include <SdFat.h>              // https://github.com/greiman/SdFat
#else                           // SdFatConfig.h内の[#define USE_LONG_FILE_NAMES 0]と[#define SDFAT_FILE_TYPE 1]は有効にしている。
#include <SD.h>
#endif
#include <TimeLib.h>            // https://github.com/PaulStoffregen/Time
#include <DS3232RTC.h>        // https://github.com/JChristensen/DS3232RTC
#endif
#include <SHthermistor.h>     // https://github.com/citriena/SHthermistor
#include <HS15P.h>            // https://github.com/citriena/humidityHS15P
#ifdef I2C_KEYPAD
#include <LiquidTWI2.h>       // https://github.com/lincomatic/LiquidTWI2
#include <simpleKeypad_I2C.h> // https://github.com/citriena/simpleKeypad_I2C
#else
#include <LiquidCrystal.h>
#include <simpleKeypad.h>     // https://github.com/citriena/simpleKeypad
#endif

///////////////////////////////////////////////////////////
//               SDカードSPI SSピン設定
///////////////////////////////////////////////////////////
#ifdef ETHERNET_SHIELD
const int chipSelect =  4;
#else
const int chipSelect = 10;
#endif
///////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////
//           使用ライブラリのコンストラクタ処理
///////////////////////////////////////////////////////////

#ifdef RTC_SD
DS3232RTC myRTC;

#ifdef SdFatLite
SdFat SD;
#endif
#endif

#ifdef I2C_KEYPAD
LiquidTWI2 lcd(0x20);     // 引数はI2Cアドレス
#else
// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(8, 9, 4, 5, 6, 7); // LCD keypad shieldにより固定
#endif

simpleKeypad keypad(200, 500);

// サーミスタによる温度測定用ライブラリのコンストラクタ
// 使用するサーミスタ、および使用条件に応じた値を以下に設定する。詳細はSHthermistorの説明参照
// 以下の数値は秋月電子通商で扱っているSEMITEC株式会社103AT-11の場合
SHthermistor thermistor(0, 25, 50, 27280, 10000, 4160, 10000, THERMISTOR_ADC, NTC_GND, THERMISTOR_EXC, 0.0);

// 湿度センサHS15Pを使用するためのライブラリのコンストラクタ
// 詳細はhumidityHS15Pの説明参照
HS15P hs15p(HS15P_OUT, HS15P_TUO, HS15P_IN, HS15P_IN_D);

///////////////////////////////////////////////////////////
//              マクロ定義
///////////////////////////////////////////////////////////

#define PARAM_RSRV_MARK  211  // EEPROMにパラメータ保存した場合に，それを示すために
// EEPROMに書き込む値。パラメータの数や定義内容を変更したら
// この値も変更する（異なる定義の設定を読み込まないようにするため）。
#define PARAM_RSRV_ADDR    0  // ctlParamを保存するEEPROMアドレス


// 以下の定数はプログラムで使う設定値の初期値。keypadで変更できる。変更値はEEPROMに保存可能。次回起動時の初期値となる。
// 設定値リセット時（RESET SETTINGS）もこの初期値となる。
#define HS15P_FACTOR    1.00   // HS15Pの個体差調整（0.50-2.00くらいか）。湿度が高く出る場合、値を上げる。
#define CTL_LOW_VPD     4.0    // 加湿開始VPD    （hPa）この値で最長加湿停止時間となる。この値以下では加湿制御を行わない。
#define CTL_HIGH_VPD    9.0    // 最大加湿VPD    （hPa）この値で最短加湿停止時間となる。これ以上VPDが大きくなっても停止時間は同じ
#define CTL_HIGH_RH    90      // 加湿開始湿度    （％）この値で最長加湿停止時間となる。これより湿度が高いと加湿制御しない。
#define CTL_LOW_RH     80      // 最大加湿湿度    （％）この値で最短加湿停止時間となる。これより湿度が低くても停止時間は同じ。
#define MIST_TIME      30      // 加湿時間       （秒）5の倍数　加湿時間は制御にかかわらず一定。挿し木床内にミストが十分行き渡る時間
#define INT_MAX_TIME  180      // 最長加湿停止時間（秒）5の倍数　開始VPD、または開始湿度のときの加湿停止時間
#define INT_MIN_TIME   30      // 最短加湿停止時間（秒）5の倍数　最大加湿VPD、または最多加湿湿度のときの加湿停止時間
#ifdef RTC_SD
#define LOG_MODE        0      // ログ記録間隔；0: 記録無; 1: 10秒, 2: 20秒, 3: 30秒, 4: 1分, 5: 2分, 6: 5分, 7: 10分
#endif
#define TEMP_COR        0.0    // 温度補正値（無補正の計測値にこの値を加算し、計測値とする。）
#define CTL_MODE        1      // 加湿制御をVPDで行う。0では湿度で行う。

#define TIME_LIMIT      300000 // 時計設定制限時間(ms)
#define MES_INT         5      // 温湿度計測間隔（秒） これにかかわらず加湿停止終了時は必ず計測し、加湿実施および休止時間を決定
#define KEY_TIMER      30      // 設定画面でキーを操作せず一定時間経過するとメイン画面に戻るまでの時間（秒）
#define MIN_MAX         5      // 最低最高温度を記憶する日数



///////////////////////////////////////////////////////////
//              型宣言
///////////////////////////////////////////////////////////

typedef enum {
  rhMODE,
  vpdMODE
} ctlMode_t;

//#define rhMODE  0
//#define vpdMODE 1


// 表示モード 9番までの内容はctlParamと同じにする。
typedef enum {
  ctlLowVpdMODE,  //  0 VPD制御時の最低VPD設定
  ctlHighVpdMODE, //  1 VPD制御時の最高VPD設定
  ctlHighRhMODE,  //  2 RH制御時の最高湿度設定
  ctlLowRhMODE,   //  3 RH制御時の最低湿度設定
  mistTimeMODE,   //  4 加湿時間（秒）設定
  intMaxTimeMODE, //  5 加湿休止最長時間（秒）設定
  intMinTimeMODE, //  6 加湿休止最短時間（秒）設定
  hs15pFtMODE,    //  7 HS15Pの補正値設定
  tempCorMODE,    //  8 温度補正値設定
  ctlMODE,        //  9 VPD制御／RH制御選択
#ifdef RTC_SD
  logMODE,        // 10 ログ間隔設定
  clkMODE,        // 11 時計設定
#endif
  stpMODE,        // 12 制御停止
  tstMODE,        // 13 加湿器動作テスト（手動動作ON）
  mesMODE,        // 14 測定（基本）
  endMODE,        // 15 測定（基本）を過ぎたことの判定用
#ifdef RTC_SD
  MIX_MODE        // 最高最低温度画面　表示手順が違うので、END_MODEの後
#endif
} dispMode_t;


typedef struct {
  int pValue;   // 設定値
  int pMax;     // 最大設定値
  int pMin;     // 最小設定値
  int pInc;     // 設定値変更幅
} ctlParam_t;

#ifdef RTC_SD
typedef struct {
  byte Month;
  byte Day;
  byte Hour;
  byte Minute;
  byte Second;
  int  Temp;        // 温度 x 10
  byte Humidity;
} logData_t;        // 全体で8バイト


typedef struct {
  byte Month;
  byte Day;
  byte minHour;
  byte minMinute;
  byte maxHour;
  byte maxMinute;
  int minTenFold;
  int maxTenFold;
} minMax_t;
#endif

#ifdef RTC_SD
#define PARAM_NUM 11      // ctlParamの要素数
#else
#define PARAM_NUM 10      // ctlParamの要素数
#endif
////////////////////////////////////////////////////////////////
//                       広域変数宣言
/////////////////////////////////////////////////////////////////
int gCtlLowVpd =  CTL_LOW_VPD * 10;
int gCtlHighVpd = CTL_HIGH_VPD * 10;
int gCtlHighRh = CTL_HIGH_RH;
int gCtlLowRh = CTL_LOW_RH;
int gMistTime = MIST_TIME;
int gIntMaxTime = INT_MAX_TIME;
int gIntMinTime = INT_MIN_TIME;
int gHs15pFactor = HS15P_FACTOR * 100;
int gTempCor = TEMP_COR * 10;
int gCtlMode = CTL_MODE;
#ifdef RTC_SD
int gLogMode = LOG_MODE;
#endif

ctlParam_t gCtlParam[PARAM_NUM] = {  // dispMode_tと同じ番号；デフォルトの設定値, 最大設定値、最小設定値、設定値変更幅
  {(int)(CTL_LOW_VPD * 10),    99,  1,  1},  // 0 VPD制御時の最低VPD（hPa）（これ以下の時は加湿停止） 使うときは /10
  {(int)(CTL_HIGH_VPD * 10),   99,  1,  1},  // 1 VPD制御時の最高VPD（hPa）（これ以上の時は最大加湿状態）使うときは /10
  {CTL_HIGH_RH,                99, 50,  1},  // 2 RH制御時の最高湿度（%）（これ以上の時は加湿停止）
  {CTL_LOW_RH,                 99, 50,  1},  // 3 RH制御時の最低湿度（%）（これ以下の時は最大加湿状態）
  {MIST_TIME,                 990,  0, 10},  // 4 加湿時間（秒）
  {INT_MAX_TIME,              990,  0, 10},  // 5 加湿休止最長時間（秒）
  {INT_MIN_TIME,              990,  0, 10},  // 6 加湿休止最短時間（秒）
  {(int)(HS15P_FACTOR * 100), 999,  1,  1},  // 7 HS15Pの補正値 使うときは /100
  {(int)((TEMP_COR) * 10),     50,-50,  1},  // 8 温度補正値 使うときは /10
  {CTL_MODE,                    1,  0,  1},  // 9 VPD制御／RH制御
#ifdef RTC_SD
  {LOG_MODE,                    8,  0,  1}   //10 ログ記録間隔
#endif
};

#ifdef RTC_SD
bool gIsSD = false;
int gLogInt[8] = {0, 10, 20, 30, 60, 120, 300, 600}; // ログ記録間隔（秒）0は記録停止
logData_t gLogData[3];                               // 8 x 3 = 24バイト　（Arduino書き込みバッファの30バイト以内）
minMax_t gMinMax[MIN_MAX];                           // 最低最高気温
byte gMinMaxPt[MIN_MAX];                             // 0:当日, 1前日のgMinMax要素番号; 99はデータなし
#endif

bool gCtlActive = true;                             // 制御有効のフラグ。手動で制御停止中はfalse
dispMode_t gDispMode = mesMODE;
bool gEditing = false;                              // 設定変更時に使用

/////////////////////////////////////////////////////////////////////////////
//                  オブジェクト
///////////////////////////////////////////////////////////
// the logging file
#ifdef RTC_SD
#ifdef SdFatLite
SdFile logfile;
#else
File logfile;            // SDカード記録に使用するファイルオブジェクト
#endif
#endif
/////////////////////////////////////////
//                  メインコード
///////////////////////////////////////////////////////////

void setup(void) {
#if defined(DEBUG) || defined(SERIAL_MONITOR)
  Serial.begin(9600);
#endif
#ifdef I2C_KEYPAD
  lcd.setMCPType(LTI_TYPE_MCP23017);
#endif
  // set up the LCD's number of rows and columns:
  lcd.begin(16, 2);                       // set up the LCD's number of columns and rows:
  if (keypad.read_buttons() == btnNONE) {  // 何もキーが押されていなかったら設定読みこみ
    readParam();
  } else {
    EEPROM.write(PARAM_RSRV_ADDR, 0xFF);   // 設定無効化
    lcd.setCursor(0, 0);
    lcd.print(F("RESET COMPLETED"));
    do {
    } while (keypad.read_buttons() != btnNONE);
  }
  lcd.clear();
#ifdef RTC_SD
  initMinMax();
  // 完全巻下げは動作モードを最初リセットモードにしているのでここでは不要
#endif
  pinMode(RH_CONT_PIN, OUTPUT);
  pinMode(ERROR_OUT, OUTPUT);
  pinMode(THERMISTOR_EXC, OUTPUT);
}


void loop(void) {

  static int keyTimer = 0;     // 一定時間キーを操作しなかったらLCD表示を計測画面にするためのタイマー変数
  static unsigned long secPrev;

  unsigned long sec = millis() / 1000;   // 経過秒を計算
  if (sec != secPrev) {        // 秒が変わったら（すなわち1秒ごとに）加湿制御ルーチンを呼び出す。
    mistCont();
    secPrev = sec;
    if (keyTimer > 0) {        // keyTimerが残っていたら
      keyTimer--;              // 1秒毎にキー操作タイマーを減らす。（一定時間操作しなかったら測定画面に移行するため）
    } else {                   // keyTimerがゼロなら（キーを操作せず一定時間経過したら）
      gDispMode = mesMODE;     // 測定画面に戻る．
    }

  }
  if (read_keypad()) {         // キー操作ルーチンはタイムラグが無いようにループ毎に呼び出す。
    keyTimer = KEY_TIMER;      // キーが押されていればキー操作タイマーをリセット
  }
}


/* 加湿器制御 */

void mistCont() {

  static int mistTime = 0; // 加湿時間のタイマー
  static int intTime = 0;  // 休止時間のタイマー
  static int mesTime = 0;  // 計測間隔のタイマー
  static int cMistTime;    // 現在の加湿時間算出値保存
  static int cIntTime;     // 現在の休止時間産出値保存
  int dMistTime;           // 加湿時間表示用
  int dIntTime;            // 休止時間表示用

  static float temp;       // 気温測定値（℃）
  static float rh;         // 相対湿度測定値（％）
#ifdef RTC_SD
  static float sumTemp = 0;// 平均処理用
  static float sumRH = 0;  // 平均処理用
  static int   dataNum = 0;// 平均処理用
#endif
  static float vpd;        // 飽差（vapor pressure deficit）測定値（hPa）
  int value;               // 測定値（飽差または湿度）を制御用に変換した数値
  int ctlLow;              // gCctlLowVpdもしくはgCtlLowRhを制御用に変換した数値
  int ctlHigh;             // gCtlHighVpdもしくはgCtlHighRhを制御用に変換した数値
  int intMaxTime;          // gIntMaxTimeを制御用に元に戻した数値
  int intMinTime;          // gIntMinTimeを制御用に元に戻した数値
  int digit = 0;           // VPDの小数点以下表示桁（10以上時の対応用）
#ifdef RTC_SD
  tmElements_t tm;
#endif

  if (mesTime == 0) {             // 計測タイマーが0なら計測
    temp = getTemp();             // 温度計測
    rh = hs15p.getRh(temp);       // 湿度計測
    vpd = hs15p.getVPD(temp, rh); // VPD算出
#ifdef RTC_SD
    sumTemp += temp;
    sumRH += rh;
    dataNum++;
#endif
    mesTime = MES_INT;            // 計測間隔タイマーをリセット
  } else {
    mesTime--;                    // 計測タイマーを減らす。
  }
#ifdef RTC_SD
  myRTC.read(tm);                       // RTCから時刻を読む
  if (isLogTime(tm)) {
    writeLog(tm, sumTemp / dataNum, sumRH / dataNum, false);
    sumTemp = 0;
    sumRH = 0;
    dataNum = 0;
  }
  minMax(tm, (int)(temp * 10 + 0.5));
#endif
  if ((mistTime == 0) && (intTime == 0)) {  // 休止時間が終了したら測定値と制御設定に基づき加湿時間と休止時間を再計算
    if (gCtlMode == vpdMODE) {              // VPD制御の場合
      ctlLow = gCtlLowVpd;                  // 10倍値なのでそのまま代入；設定変更した場合に対応するために毎回設定
      ctlHigh = gCtlHighVpd;                // 10倍値なのでそのまま代入；設定変更した場合に対応するために毎回設定
      value = int(vpd * 10);                // 制御用に10倍に変換
    } else {                                // RH制御の場合; VPDとは上下が逆なので、同じ制御ができるように変換
      ctlLow = gCtlLowRh;    //
      ctlHigh = gCtlHighRh;  //
      value = ctlLow + (ctlHigh - int(rh)); // 制御用に変換
    }
    if (value < ctlLow) {                   // 測定値が制御値以下だったら
      mistTime = 0;                         // ミスト時間はゼロ（加湿しない）。
      cMistTime = 0;                        // ミスト時間算出値もゼロ
      intTime = 0;                          // 休止時間はゼロ
      cIntTime = 0;                         // 休止時間算出値もゼロ
    } else {                                // 測定値が制御値を超えている場合は加湿時間および休止時間を設定する。
      mistTime = gMistTime;                 // キーで設定変更した場合に対応するために毎回設定
      cMistTime = mistTime;                 // 設定値を代入保存
      intMaxTime = gIntMaxTime;             // キーで設定変更した場合に対応するために毎回設定 初期値：180秒
      intMinTime = gIntMinTime;             // キーで設定変更した場合に対応するために毎回設定 初期値： 20秒
      if (value >= ctlHigh) {               // 乾燥程度が最大値を超えていたら
        intTime = intMinTime;               // 最短加湿停止時間
      } else {                              // そうでないなら、加湿停止時間を計算（単純比例）
        intTime = intMinTime +  int((intMaxTime - intMinTime) *  (float(ctlHigh - value) / float(ctlHigh - ctlLow)));
      }
      cIntTime = intTime;                   // 算出した休止時間を保存
    }
  } // 制御パラメータの算出完了

  if (gCtlActive) {                      // 制御有効な場合、計算結果に基づき加湿のON／OFFを行う。
    if (mistTime > 0) {                  // 加湿中であれば
      mistTime--;                        // 加湿タイマーを減らす。
      dMistTime = mistTime;              // 加湿中は現在の加湿タイマーを表示する。
      dIntTime = cIntTime;               // 加湿中は休止タイマーは設定値を表示する。
      digitalWrite(RH_CONT_PIN, HIGH);   // 加湿中なので出力はHIGH
    } else {                             // 加湿休止中であれば
      if (intTime > 0) {                 // 休止タイマー実行中なら
        intTime--;                       // タイマーを減らす。
        if (intTime == 0) mesTime = 0;   // 休止タイマー終了だったら次の計測タイムを待たずに計測
      }
      dIntTime = intTime;                // 加湿休止中は、現在の休止タイマー値を表示する。
      dMistTime = cMistTime;             // 加湿休止中は、加湿タイマーは0ではなく、設定値を表示する。
      digitalWrite(RH_CONT_PIN, LOW);    // 加湿休止中 なので出力LOW
    }
  } else {                               // 制御手動停止中
    mistTime = 0;
    intTime = 0;
    dMistTime = cMistTime;               // 表示は設定による計算値
    dIntTime = cIntTime;                 // 表示は設定による計算値
    digitalWrite(RH_CONT_PIN, LOW);
  } // 加湿制御実行完了


  /* LCD出力 */
  if (gDispMode == mesMODE) {            // 測定画面だったら
    lcd.setCursor(0, 0);
    //    lcd.clear();
    if (temp == 999) {
      lcd.print("ERROR");
    } else {
      if (temp < 9.95) lcd.print(F(" "));  // 10℃以下時は桁揃えのためにスペースを置く
      lcd.print( temp , 1 );
//      lcd.print(F("C,"));
      lcd.write(0xdf);
    }
    float tRh = rh;
    if (tRh > 99.4) tRh = 0;
    if (tRh <  9.5) lcd.print(F("0"));
    lcd.print( tRh , 0 );
    lcd.print(F("%,D"));
    float tVpd = vpd;
    if (tVpd < 9.95) digit = 1;
    lcd.print( tVpd , digit);
    lcd.print(F("hPa"));
    lcd.setCursor(0, 1);
    lcd.print(F("M"));
    if (dMistTime < 100) lcd.print(F("0"));
    if (dMistTime <  10) lcd.print(F("0"));
    lcd.print(dMistTime);
    lcd.print(F(",I"));
    if (dIntTime < 100) lcd.print(F("0"));
    if (dIntTime <  10) lcd.print(F("0"));
    lcd.print(dIntTime);
    lcd.print(F(",C"));
    if (gCtlMode == vpdMODE) {
      lcd.print(float(gCtlLowVpd) / 10.0, 1);
      lcd.print(F("hP"));
    } else {
      lcd.print(gCtlHighRh);
      lcd.print(F("%"));
    }
  }
#ifdef DEBUG
  Serial.print("temp=");
  Serial.print(temp, 1);
  Serial.print(",rh=");
  Serial.print(rh, 1);
  Serial.print("vpd=");
  Serial.print(vpd, 1);
  Serial.print(",mist=");
  Serial.print(mistTime);
  Serial.print(",int=");
  Serial.println(intTime);
#endif
}


boolean read_keypad() {
  btnCODE_t lcd_key;
  static char dayBefore = -1; // -1は最高最低ではなく基本画面

  lcd_key = keypad.read_buttons(); // read the buttons
  switch (lcd_key) {               // 押されたキーに応じた共通処理（画面遷移、設定値の変更）
    case btnRIGHT: {
#ifdef RTC_SD
      if (gDispMode == MIX_MODE) {
        gDispMode = mesMODE;        // 基本画面に戻るだけ
        dayBefore = -1;
        break;
      }
#endif
      gDispMode = (dispMode_t)static_cast<int>(gDispMode + 1);     // gDispMode++ が使えないので変則的な方法
      if (gDispMode == endMODE) {   // 最終画面判定値だったら最初に戻る。
        gDispMode = (dispMode_t)(0);
      }
      if ((gCtlMode == vpdMODE) && (gDispMode == ctlHighRhMODE)) { // 加湿制御値設定時は制御モードによって画面を変える。
        gDispMode = mistTimeMODE;
      }
      if ((gCtlMode == rhMODE) && (gDispMode == ctlLowVpdMODE)) {  // 加湿制御値設定時は制御モードによって画面を変える。
        gDispMode = ctlHighRhMODE;
      }
      gEditing = false;               // 画面を移動したら編集モードをリセット
      break;
    }
    case btnLEFT: {
#ifdef RTC_SD
      if (gDispMode == MIX_MODE) {
        gDispMode = mesMODE;        // 基本画面に戻るだけ
        dayBefore = -1;
        break;
      }
#endif
      if (gDispMode == 0) {
        gDispMode = mesMODE;
      } else {
        gDispMode = (dispMode_t)static_cast<int>(gDispMode - 1);   // gDispMode-- が使えないので変則的な方法
      }
      if ((gCtlMode == vpdMODE) && (gDispMode == ctlLowRhMODE)) {  // 加湿制御値設定時は制御モードによって画面を変える。
        gDispMode = ctlHighVpdMODE;
      }
      if ((gCtlMode == rhMODE) && (gDispMode == ctlHighVpdMODE)) { // 加湿制御値設定時は制御モードによって画面を変える。
        gDispMode = mesMODE;
      }
      gEditing = false;               // 画面を移動したら編集モードをリセット
      break;
    }
    case btnUP: {
      if (gDispMode < PARAM_NUM) { // パラメータ変更画面のgDispMode番号はctlParamと同じ番号で、gDispModeのはじめの方にまとめている。
        gCtlParam[gDispMode].pValue += gCtlParam[gDispMode].pInc;
        if (gCtlParam[gDispMode].pValue > gCtlParam[gDispMode].pMax) {
          gCtlParam[gDispMode].pValue = gCtlParam[gDispMode].pMax;
        }
        break;
#ifdef RTC_SD
      } else if ((gDispMode == mesMODE) || (gDispMode == MIX_MODE)){
          gDispMode = MIX_MODE;
          if (dayBefore < (MIN_MAX - 1)) {
            if (gMinMax[gMinMaxPt[dayBefore + 1]].Day > 0) {
              dayBefore++;
            }
          }
        break;
#endif
      }
      return false;
    }
    case btnDOWN: {
      if (gDispMode < PARAM_NUM) { // パラメータ変更画面のgDispMode番号はctlParamと同じ番号で、gDispModeのはじめの方にまとめている。
        gCtlParam[gDispMode].pValue -= gCtlParam[gDispMode].pInc;
        if (gCtlParam[gDispMode].pValue < gCtlParam[gDispMode].pMin) {
          gCtlParam[gDispMode].pValue = gCtlParam[gDispMode].pMin;
        }
        break;
#ifdef RTC_SD
      } else if (gDispMode == MIX_MODE){
        gDispMode = MIX_MODE;
        if (dayBefore >= 0) dayBefore--;
        if  (dayBefore == -1) gDispMode = mesMODE; 
        break;
#endif
      }
      return false;
    }
    case btnSELECT: { // SELECTの処理は共通化できないので次の画面表示処理内で行う。
      break;
    }
    case btnVOID: {   // 画面表示変更しないのでそのまま戻る。
      return false;
    }
    case btnNONE: {   // 画面表示変更しないのでそのまま戻る。
      return false;
    }
  }

  lcd.setCursor(0, 0);
  lcd.clear();
  switch (gDispMode) { // 個別の画面表示、および共通化できない画面毎の個別処理
    case hs15pFtMODE: {
      dispHs15pFtMode(lcd_key);
      break;
    }
    case ctlHighRhMODE: {
      dispCtlHighRhMode(lcd_key);
      break;
    }
    case ctlLowRhMODE: {
      dispCtlLowRhMode(lcd_key);
      break;
    }
    case ctlLowVpdMODE: {
      dispCtlLowVpdMode(lcd_key);
      break;
    }
    case ctlHighVpdMODE: {
      dispCtlHighVpdMode(lcd_key);
      break;
    }
    case mistTimeMODE: {
      dispMistTimeMode(lcd_key);
      break;
    }
    case intMaxTimeMODE: {
      dispIntMaxTimeMode(lcd_key);
      break;
    }
    case intMinTimeMODE: {
      dispIntMinTimeMode(lcd_key);
      break;
    }
    case tempCorMODE: {
      dispTempCorMode(lcd_key);
      break;
    }
    case ctlMODE: {     // 制御方法設定（VPDを使うか，RHを使うか）
      dispCtlMode(lcd_key);
      break;
    }
#ifdef RTC_SD
    case logMODE: {     // ログ間隔
      dispLogMode(lcd_key);
      break;
    }
    case clkMODE: {     // 制御方法設定（VPDを使うか，RHを使うか）
      dispClkMode(lcd_key);
      break;
    }
    case MIX_MODE:
      dispMinMax(dayBefore);
      break;
#endif
    case stpMODE: {       // 加湿制御手動停止
      dispStpMode(lcd_key);
      break;
    }
    case tstMODE: {  // 加湿器テスト（手動動作）
      lcd.print(F("MIST TEST     "));
      lcd.setCursor(0, 1);
      lcd.print(F("SELECT=YES"));
      if (lcd_key == btnSELECT) {
        lcd.setCursor(0, 1);
        lcd.print(F("PRESS TO STOP"));
        digitalWrite(RH_CONT_PIN, HIGH);
        while (keypad.read_buttons() != btnNONE); // これが無いと直前のキーの影響で直ぐ終了する。
        unsigned long ts = millis() + (30 * 1000);  // 30秒経ったら終了
        while (keypad.read_buttons() == btnNONE) {
          lcd.setCursor(14, 0);
          int rest = (ts - millis()) / 1000;
          if (rest < 10) lcd.print(F("0"));
          lcd.print(rest, DEC);
          if ((millis() > ts) || (millis() < 1000)) break;
        }
        digitalWrite(RH_CONT_PIN, LOW);
        gDispMode = mesMODE; // 終了したら直ぐに測定画面に戻る。
        return false; // キーが押されたと判定されたら画面が変わらないので押されていないとして戻る。
      }
      break;
    }
    case mesMODE: {
      return false;  // キーは押されていないとして戻る。
    }
    case endMODE: {
      return false; // キーは押されていないとして戻る。
    }
  }
  return true;
}


void dispHs15pFtMode(btnCODE_t lcd_key) {
  if (!gEditing) {
    gCtlParam[gDispMode].pValue = gHs15pFactor;
    gEditing = true;
  }
  lcd.print(F("HS15P  F="));
  lcd.print((float)gCtlParam[gDispMode].pValue / 100, 2);
  dispSelect();
  if (lcd_key == btnSELECT) {
    gHs15pFactor = gCtlParam[gDispMode].pValue;
    hs15p.setFactor(float(gHs15pFactor) / 100.0);
    writeParam();
    gDispMode = mesMODE;
  }
};


void dispCtlHighRhMode(btnCODE_t lcd_key) {
  if (!gEditing) {
    gCtlParam[gDispMode].pValue = gCtlHighRh;
    gEditing = true;
  }
  if (gCtlParam[gDispMode].pValue < gCtlLowRh) {
    gCtlParam[gDispMode].pValue = gCtlLowRh;
  }
  lcd.print(F("RH HIGH ="));
  lcd.print(gCtlParam[gDispMode].pValue);
  lcd.print(F("%"));
  dispSelect();
  if (lcd_key == btnSELECT) {
    gCtlHighRh = gCtlParam[gDispMode].pValue;
    writeParam();
    gDispMode = mesMODE;
  }
}


void dispCtlLowRhMode(btnCODE_t lcd_key) {
  if (!gEditing) {
    gCtlParam[gDispMode].pValue = gCtlLowRh;
    gEditing = true;
  }
  if (gCtlParam[gDispMode].pValue > gCtlHighRh) {
    gCtlParam[gDispMode].pValue = gCtlHighRh;
  }
  lcd.print(F("RH LOW  ="));
  lcd.print(gCtlParam[gDispMode].pValue);
  lcd.print(F("%"));
  dispSelect();
  if (lcd_key == btnSELECT) {
    gCtlLowRh = gCtlParam[gDispMode].pValue;
    writeParam();
    gDispMode = mesMODE;
  }
}


void dispCtlLowVpdMode(btnCODE_t lcd_key) {
  if (!gEditing) {
    gCtlParam[gDispMode].pValue = gCtlLowVpd;
    gEditing = true;
  }
  if (gCtlParam[gDispMode].pValue > gCtlHighVpd) {
    gCtlParam[gDispMode].pValue = gCtlHighVpd;
  }
  lcd.print(F("VPD LOW ="));
  lcd.print(float(gCtlParam[gDispMode].pValue) / 10.0, 1);
  lcd.print(F("hPa"));
  dispSelect();
  if (lcd_key == btnSELECT) {
    gCtlLowVpd = gCtlParam[gDispMode].pValue;
    writeParam();
    gDispMode = mesMODE;
  }
}


void dispCtlHighVpdMode(btnCODE_t lcd_key) {
  if (!gEditing) {
    gCtlParam[gDispMode].pValue = gCtlHighVpd;
    gEditing = true;
  }
  if (gCtlParam[gDispMode].pValue < gCtlLowVpd) {
    gCtlParam[gDispMode].pValue = gCtlLowVpd;
  }
  lcd.print(F("VPD HIGH="));
  lcd.print(float(gCtlParam[gDispMode].pValue) / 10.0, 1);
  lcd.print(F("hPa"));
  dispSelect();
  if (lcd_key == btnSELECT) {
    gCtlHighVpd = gCtlParam[gDispMode].pValue;
    writeParam();
    gDispMode = mesMODE;
  }
}


void dispMistTimeMode(btnCODE_t lcd_key) {
  if (!gEditing) {
    gCtlParam[gDispMode].pValue = gMistTime;
    gEditing = true;
  }
  lcd.print(F("MIST    ="));
  lcd.print(gMistTime);
  lcd.print("s");
  dispSelect();
  if (lcd_key == btnSELECT) {
    gMistTime = gCtlParam[gDispMode].pValue;
    writeParam();
    gDispMode = mesMODE;
  }
}


void dispIntMaxTimeMode(btnCODE_t lcd_key) {
  if (!gEditing) {
    gCtlParam[gDispMode].pValue = gIntMaxTime;
    gEditing = true;
  }
  if (gCtlParam[gDispMode].pValue < gIntMinTime) {
    gCtlParam[gDispMode].pValue = gIntMinTime;
  }
  lcd.print(F("INT MAX ="));
  lcd.print(gIntMaxTime);
  lcd.print(F("s"));
  dispSelect();
  if (lcd_key == btnSELECT) {
    gIntMaxTime = gCtlParam[gDispMode].pValue;
    writeParam();
    gDispMode = mesMODE;
  }
}


void dispIntMinTimeMode(btnCODE_t lcd_key) {
  if (!gEditing) {
    gCtlParam[gDispMode].pValue = gIntMinTime;
    gEditing = true;
  }
  if (gCtlParam[gDispMode].pValue > gIntMaxTime) {
    gCtlParam[gDispMode].pValue = gIntMaxTime;
  }
  lcd.print(F("INT MIN ="));
  lcd.print(gCtlParam[gDispMode].pValue);
  lcd.print(F("s"));
  dispSelect();
  if (lcd_key == btnSELECT) {
    gIntMinTime = gCtlParam[gDispMode].pValue;
    writeParam();
    gDispMode = mesMODE;
  }
}


void dispTempCorMode(btnCODE_t lcd_key) {
  if (!gEditing) {
    gCtlParam[gDispMode].pValue = gTempCor;
    gEditing = true;
  }
  lcd.print(F("TEMP CORR="));
  if (gCtlParam[gDispMode].pValue >= 0) {
    lcd.print(F("+"));
  }
  lcd.print(float(gCtlParam[gDispMode].pValue) / 10.0, 1);
  lcd.write(0xdf);
//  lcd.print(F("C"));
  dispSelect();
  if (lcd_key == btnSELECT) {
    gTempCor = gCtlParam[gDispMode].pValue;
    writeParam();
    gDispMode = mesMODE;
  }
}


void dispCtlMode(btnCODE_t lcd_key) {
  if (!gEditing) {
    gCtlParam[gDispMode].pValue = gCtlMode;
    gEditing = true;
  }
  lcd.print(F("CTL MODE="));
  if (gCtlParam[gDispMode].pValue == vpdMODE) {
    lcd.print(F("VPD"));
  } else {
    lcd.print(F("RH "));
  }
  dispSelect();
  if (lcd_key == btnSELECT) {
    gCtlMode = gCtlParam[gDispMode].pValue;
    writeParam();
    gDispMode = mesMODE;
  }
}


#ifdef RTC_SD

void dispLogMode(btnCODE_t lcd_key) {
  tmElements_t tm;

  if (!gEditing) {
    gCtlParam[gDispMode].pValue = gLogMode;
    gEditing = true;
  }
  int logInt = gLogInt[gCtlParam[gDispMode].pValue];     // ログ間隔（秒）

  lcd.print(F("LOG INT= "));    // SDがない場合は0にするので、判定後に表示
  bool isSec = true;
  if (logInt >= 60) {
    logInt /= 60;
    isSec = false;
  }
  lcd.print(logInt);
  if (isSec) {
    lcd.print(F("sec."));
  } else {
    lcd.print(F("min."));
  }
  dispSelect();
  if (lcd_key == btnSELECT) {
    gLogMode = gCtlParam[gDispMode].pValue;
    writeParam();
    gDispMode = mesMODE;
    if (logInt == 0) {
      if (gIsSD) {
        writeLog(tm, 0, 0, true);
        logfile.close();
        gIsSD = false;
      }
    } else {
      if (!gIsSD) {
        if (setupSD()) {
          gIsSD = true;
        } else {        // 再起動時は設定したログモードになるように、この処理はパラメータ保存後におこなう。
          gLogMode = 0;
          logInt = 0;
        }
      }
    }
  }
}


void dispClkMode(btnCODE_t lcd_key) {
  tmElements_t tm;

  if (lcd_key == btnSELECT) {
    setRTCwKeypad(); // キーパッドで時計の設定
  } else {
    myRTC.read(tm);
    lcdDayTime(tm, 0, -1);
    lcd.setCursor(0, 1);
    lcd.print(F("SELECT=SET TIME"));
  }
}


void dispMinMax(char dayBefore) {
  lcd.setCursor(0, 0);
  if (gMinMax[gMinMaxPt[dayBefore]].Day == 0) {
    lcd.print(F("NO DATA"));
    return;
  }
  lcdDay(gMinMax[gMinMaxPt[dayBefore]].Month, gMinMax[gMinMaxPt[dayBefore]].Day);
  lcd.print(F(" "));
  lcdTime(gMinMax[gMinMaxPt[dayBefore]].maxHour, gMinMax[gMinMaxPt[dayBefore]].maxMinute);
  lcd.print(F(" "));
  if (gMinMax[gMinMaxPt[dayBefore]].maxTenFold < 100) lcd.print(F(" "));
  lcd.print((float)gMinMax[gMinMaxPt[dayBefore]].maxTenFold / 10, 1);
  lcd.setCursor(0, 1);
  lcd.print(dayBefore + 0x30, 0);
  lcd.setCursor(6, 1);
  lcdTime(gMinMax[gMinMaxPt[dayBefore]].minHour, gMinMax[gMinMaxPt[dayBefore]].minMinute);
  lcd.print(F(" "));
  if ((gMinMax[gMinMaxPt[dayBefore]].minTenFold < 100) && (gMinMax[gMinMaxPt[dayBefore]].minTenFold >= 0)) {
    lcd.print(F(" "));
  }
  lcd.print((float)gMinMax[gMinMaxPt[dayBefore]].minTenFold / 10, 1);
}
//0123456789ABCDEF
//10/25 13:00 25.2
//      06:00 18.5

#endif

void dispStpMode(btnCODE_t lcd_key) {
  if (lcd_key == btnSELECT) {
    gCtlActive = !gCtlActive;
  }
  if (gCtlActive) {
    lcd.print(F("STOP CONTROL    "));
  } else {
    lcd.print(F("START CONTROL   "));
  }
  lcd.setCursor(0, 1);
  lcd.print(F("SELECT=YES"));
}


void dispSelect() {
  lcd.setCursor(0, 1);
  lcd.print(F("SELECT TO SET"));
}


float getTemp() {     // 温度計測
  float temp;         // 摂氏値( ℃ )

  temp = thermistor.readTemp();
  if ((temp == TH_SHORT_ERR) || (temp == TH_OPEN_ERR)) {    // 断線、短絡の場合等
    // Serial.print("ERR:");
    // Serial.println(thermistor.readR());
    //    errorDetect("Temp error");
    return 999;
  }
  temp += (float(gTempCor) / 10.0);   //温度補正
  return temp;
}


void readParam() {
  int i;
  if (EEPROM.read(PARAM_RSRV_ADDR) == PARAM_RSRV_MARK) {
    for (i = 0; i < PARAM_NUM; i++) {
      EEPROM.get(PARAM_RSRV_ADDR + 1 + (i * sizeof(gCtlParam[1].pValue)), gCtlParam[i].pValue);
    }
    gCtlLowVpd =   gCtlParam[0].pValue;
    gCtlHighVpd =  gCtlParam[1].pValue;
    gCtlHighRh =   gCtlParam[2].pValue;
    gCtlLowRh =    gCtlParam[3].pValue;
    gMistTime =    gCtlParam[4].pValue;
    gIntMaxTime =  gCtlParam[5].pValue;
    gIntMinTime =  gCtlParam[6].pValue;
    gHs15pFactor = gCtlParam[7].pValue;
    gTempCor =     gCtlParam[8].pValue;
    gCtlMode =     gCtlParam[9].pValue;
#ifdef RTC_SD
    gLogMode =     gCtlParam[10].pValue;
#endif
    hs15p.setFactor((float)gHs15pFactor / 100.0);  // HS15P の補正
  }
}


void writeParam() {
  int i;

  gCtlParam[0].pValue = gCtlLowVpd;
  gCtlParam[1].pValue = gCtlHighVpd;
  gCtlParam[2].pValue = gCtlHighRh;
  gCtlParam[3].pValue = gCtlLowRh;
  gCtlParam[4].pValue = gMistTime;
  gCtlParam[5].pValue = gIntMaxTime;
  gCtlParam[6].pValue = gIntMinTime;
  gCtlParam[7].pValue = gHs15pFactor;
  gCtlParam[8].pValue = gTempCor;
  gCtlParam[9].pValue = gCtlMode;
#ifdef RTC_SD
  gCtlParam[10].pValue = gLogMode;
#endif
  EEPROM.write(PARAM_RSRV_ADDR, PARAM_RSRV_MARK);
  for (i = 0; i < PARAM_NUM; i++) {
    EEPROM.put(PARAM_RSRV_ADDR + 1 + (i * sizeof(gCtlParam[0].pValue)), gCtlParam[i].pValue);
  }
}


void errorDetect(String msg) {
  Serial.println(msg);
  //  digitalWrite(ERROR_OUT, HIGH);
  //  lcd.clear();
  //  lcd.setCursor(0, 0);
  //  lcd.print(msg);
  //  delay(5000);
}

#ifdef RTC_SD

void minMax(tmElements_t tm, int tempTenFold) {
  static byte prevDay = 0;

  if (prevDay != tm.Day) {
    prevDay = tm.Day;
    shiftMinMax();
    gMinMax[gMinMaxPt[0]].Month = tm.Month;
    gMinMax[gMinMaxPt[0]].Day = tm.Day;
  }
  if (tempTenFold > gMinMax[gMinMaxPt[0]].maxTenFold) {
    gMinMax[gMinMaxPt[0]].maxTenFold = tempTenFold;
    gMinMax[gMinMaxPt[0]].maxHour = tm.Hour;
    gMinMax[gMinMaxPt[0]].maxMinute = tm.Minute;
  }
  if (tempTenFold < gMinMax[gMinMaxPt[0]].minTenFold) {
    gMinMax[gMinMaxPt[0]].minTenFold = tempTenFold;
    gMinMax[gMinMaxPt[0]].minHour = tm.Hour;
    gMinMax[gMinMaxPt[0]].minMinute = tm.Minute;
  }
}


void shiftMinMax() {  // ５日前のデータ領域を今日のデータ領域にし、その他は１日ずらす。
  byte newMinMaxPt = gMinMaxPt[MIN_MAX - 1];  // 最後のデータ領域を記録しておき、新しいデータ用に使う。
  byte i;

  for (i = MIN_MAX - 1; i > 0; i--) {
    gMinMaxPt[i] = gMinMaxPt[i - 1];          // データを1日ずらす。
  }
  gMinMaxPt[0] = newMinMaxPt;                 // 最後日のデータだった領域を今日のデータ領域にする。
  gMinMax[newMinMaxPt].minTenFold =  999;     // x10なので 99.9°
  gMinMax[newMinMaxPt].maxTenFold = -999;     // x10なので-99.9°
  gMinMax[newMinMaxPt].Day = 0;
}


void initMinMax() {
  for (byte i = 0; i < MIN_MAX; i++) {
    gMinMaxPt[i] = i;     // 最初は番号通り
    gMinMax[i].Day = 0;   // 0はデータ無しを示す。
  }
}


bool isLogTime(tmElements_t tm) {
  byte tms;
  static byte loggedTime = 99;

  if (!gIsSD) return false;
  int logInt = gLogInt[gLogMode];     // ログ間隔（秒）
  if (logInt == 0) return false;
  if (logInt < 60) {  // ログ間隔が1分未満時は秒で処理
    tms = tm.Second;
  } else {            // ログ間隔が1分以上時は分で処理
    tms = tm.Minute;
    logInt /= 60;
  }
  if ((tms % logInt) == 0) {     // この方法では1秒間隔のログは出来ない。
    if (tms != loggedTime) {     // 未ログの場合
      loggedTime = tms;          // ログフラグを立てる
      return true;               // ログ時間としてリターン
    }
  }
  return false;
}

void writeLog(tmElements_t tm, float temp, float humidity, bool isFlush) {
  static byte bufNo = 0;
  if (!isFlush) {
    gLogData[bufNo].Month  = tm.Month;
    gLogData[bufNo].Day    = tm.Day;
    gLogData[bufNo].Hour   = tm.Hour;
    gLogData[bufNo].Minute = tm.Minute;
    gLogData[bufNo].Second = tm.Second;
    gLogData[bufNo].Temp   = (int)((temp * 10) + 0.5);
    gLogData[bufNo].Humidity = (byte)(humidity + 0.5);
    bufNo++;
  }
  if ((bufNo >= 3) || isFlush) {
    writeBuf(bufNo);
    bufNo = 0;
  }
}


void writeBuf(byte bufNo) {
  for (byte i = 0; i < bufNo; i++) {
    logfile.print(gLogData[i].Month);
    logfile.print(F("/"));
    logfile.print(gLogData[i].Day);
    logfile.print(F(","));
    logfile.print(gLogData[i].Hour);
    logfile.print(F(":"));
    logfile.print(gLogData[i].Minute);
    logfile.print(F(":"));
    logfile.print(gLogData[i].Second);
    logfile.print(F(","));
    logfile.print((float)gLogData[i].Temp / 10, 1);
    logfile.print(F(","));
    logfile.println(gLogData[i].Humidity);
  }
}



/////////////////////////////////////////////////////////////
//                    SDカードの設定
/////////////////////////////////////////////////////////////
// SDカードのイニシャライズ
// https://github.com/adafruit/Light-and-Temp-logger/blob/master/lighttemplogger.ino
bool setupSD() {
  // initialize the SD card
#ifdef SERIAL_MONITOR
  Serial.print(F("Initializing SD card..."));
#endif
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(chipSelect, OUTPUT);

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
#ifdef SERIAL_MONITOR
    Serial.println(F("Card failed, or not present"));
#endif
    // don't do anything more:
    return false;
  }
#ifdef SERIAL_MONITOR
  Serial.println(F("card initialized."));
#endif
  // create a new file
  bool isFileOpen;
  char filename[] = "LOGGER00.CSV";
  SdFile::dateTimeCallback( &dateTime );
  for (uint8_t i = 0; i < 100; i++) {
    filename[6] = i / 10 + '0';
    filename[7] = i % 10 + '0';
    if (! SD.exists(filename)) {
      // only open a new file if it doesn't exist
#ifdef SdFatLite
      isFileOpen = logfile.open(filename, O_WRONLY | O_CREAT | O_EXCL);
#else
      logfile = SD.open(filename, FILE_WRITE);
#endif
      break;  // leave the loop!
    }
  }
#ifdef SdFatLite
  if (!isFileOpen) {
#else
  if (!logfile) {
#endif
#ifdef SERIAL_MONITOR
    Serial.println(F("couldnt create file"));
#endif
    return false;
  }
#ifdef SERIAL_MONITOR
  Serial.print(F("Logging to: "));
  Serial.println(filename);
#endif
  //  logfile.close();
  return true;
}


// SDカードのファイル作成日付設定用
// 色んな所で使われているコードで、どれがオリジナルかは不明
void dateTime(uint16_t* date, uint16_t* time) {

  //setSyncProvider(myRTC.get()); //sync time with RTC
  tmElements_t tm;
  myRTC.read(tm);          // RTCから時刻を読む

  // FAT_DATEマクロでフィールドを埋めて日付を返す
  // fill date to FAT_DATE macro
  *date = FAT_DATE(tmYearToCalendar(tm.Year), tm.Month, tm.Day);

  // FAT_TIMEマクロでフィールドを埋めて時間を返す
  // fill time to FAT_TIME macro
  *time = FAT_TIME(tm.Hour, tm.Minute, tm.Second);
}


/////////////////////////////////////////////////////////////
//            LCD、キーパッドを使った時計の表示、設定
/////////////////////////////////////////////////////////////
//////////////////// RTC SET ////////////////////////
// set RTC clock using LCD keypad shield           //
/////////////////////////////////////////////////////
void setRTCwKeypad() {
  //  int i;
  byte tStart[5] = {CalendarYrToTm(2022), 1, 1, 0, 0}; // Year member of tmElements_t is an offset from 1970
  byte tValue[5];
  byte tLimit[5] = {CalendarYrToTm(2099), 12, 31, 23, 59};
  byte item = 0; // year=0, month=1, day=2, hour=3, minute=4
  bool isChanged = false;        //日時の修正有無
  byte lcd_key;                  // キー入力コード
  unsigned long waitTime = millis() + TIME_LIMIT;  // TIME_LIMITms経過したら時計設定完了していなくても終了
  tmElements_t tm;

  myRTC.read(tm);          // RTCから時刻を読む
  tValue[0] = tm.Year;
  tValue[1] = tm.Month;
  tValue[2] = tm.Day;
  tValue[3] = tm.Hour;
  tValue[4] = tm.Minute;

  byte cursorColumn[5] = {0, 5, 8, 0x0b, 0x0e};
  // |    |  |  |  |
  // 0123456789abcdef
  // 2019/07/01 16:15

  lcd.clear();
  lcdDayTime(tm, 0, cursorColumn[item]);
  do {
    if (item == 2) tLimit[2] = daysInMonth(tm); // last day number in the month
    lcd_key = keypad.read_buttons(); // read the buttons
    switch (lcd_key) {               // depending on which button was pushed, we perform an action
      case btnRIGHT: {
          if (item < 4) {
            item++;
          }
          break;
        }
      case btnLEFT: {
          if (item > 0) {
            item--;
          }
          break;
        }
      case btnUP: {
          if (tValue[item] >= tLimit[item]) {
            tValue[item] = tStart[item]; // value will roll over from the start point if reaches the limit
          } else {
            tValue[item]++;              // increase the value if button is pressed
          }
          isChanged = true;
          break;
        }
      case btnDOWN: {
          if (tValue[item] <= tStart[item]) {
            tValue[item] = tLimit[item]; // value will roll over to the limit point if reaches the start point.
          } else {
            tValue[item]--;              // if button is pressed increase the value
          }
          isChanged = true;
          break;
        }
      case btnSELECT: {
          break;
        }
      case btnNONE: {
          break;
        }
    }
    if (lcd_key != btnNONE) {
      tm = {0, tValue[4], tValue[3], 0, tValue[2], tValue[1], tValue[0]}; //設定値をtmに代入したら whileに戻って表示
      lcdDayTime(tm, 0, cursorColumn[item]);
    }
    if (waitTime < millis()) {             // 指定時間を超えたら自動的に設定終了
      lcd_key = btnSELECT;
#ifdef SERIAL_MONITOR
      Serial.println(F("Time Up"));
#endif
    }
  } while (lcd_key != btnSELECT);
  if (isChanged) myRTC.write(tm);            // 設定値をRTCに設定。ゼロ秒で押せば秒まで正確に設定できる。
  lcd.noBlink();
}


// うるう年を考慮して各月の日数を計算
byte daysInMonth(tmElements_t tm) {
  byte days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; //days of eath month
  if (!(tmYearToCalendar(tm.Year) % 4)) {   // leap year
    days[1] = 29;
  }
  return days[tm.Month - 1];
}


// LCDに日時を表示
void lcdDayTime(tmElements_t tm, byte row, char cursorColumn) {
  lcd.setCursor(0, row);
  lcd.print(tmYearToCalendar(tm.Year));
  lcd.print(F("/"));
  lcdDay(tm.Month, tm.Day);
  lcd.print(F(" "));
  lcd.setCursor(11, row);
  lcdTime(tm.Hour, tm.Minute);
//  if (tm.Hour < 10) lcd.print(F("0"));
//  lcd.print(tm.Hour, DEC);
//  lcd.print(F(":"));
//  if (tm.Minute < 10) lcd.print(F("0"));
//  lcd.print(tm.Minute, DEC);
  if (cursorColumn >= 0) {
    lcd.setCursor(cursorColumn, 0);
    lcd.blink();
  }
}


void lcdDay(byte mt, byte dy) {
  if (mt < 10) lcd.print(F("0"));
  lcd.print(mt, DEC);
  lcd.print(F("/"));
  if (dy < 10) lcd.print(F("0"));
  lcd.print(dy, DEC);
}


void lcdTime(byte hr, byte mn) {
  if (hr < 10) lcd.print(F("0"));
  lcd.print(hr, DEC);
  lcd.print(F(":"));
  if (mn < 10) lcd.print(F("0"));
  lcd.print(mn, DEC);
}

#endif