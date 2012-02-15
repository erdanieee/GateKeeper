#include <16F88.h>
#device adc=10
#use delay(clock=4000000)

#FUSES NOWDT                    //No Watch Dog Timer
#FUSES INTRC                    //Internal RC Osc
#FUSES PUT                      //Power Up Timer
#FUSES NOMCLR                   //Master Clear pin disabled
#FUSES BROWNOUT                 //brownout reset
#FUSES NOLVP                    //No low voltage prgming, B3(PIC16) or B5(PIC18) used for I/O
#FUSES NOCPD                    //No EE protection
#FUSES NOWRT                    //Program memory not write protected
#FUSES NODEBUG                  //No Debug mode for ICD
#FUSES NOPROTECT                //Code not protected from reading
#FUSES FCMEN                    //Fail-safe clock monitor enabled
#FUSES IESO                     //Internal External Switch Over mode enabled


#define ON              TRUE
#define OFF             FALSE
#define INT_ONKEY_ON    L_TO_H
#define INT_ONKEY_OFF   H_TO_L

#define TIME_DEBOUNCE      20
#define TIME_BT_ON         100           //TODO: probar a reducir el tiempo de espera!
#define TIMEOUT_COUNT      200           //~10s@4MHz (waiting bt paired or bt cmd ok should not spend so long!!)
#define TIME_PAIRED        28036         //300ms@4MHz,DIV_8
#define TIME_BAT_FUEL      60            //~50ms@4MHz,DIV_256 
#define TIME_SHORT_NOTE    15
#define TIME_LONG_NOTE     30
#define BT_BUFF_SIZE       40
#define BAT_BUFF_SIZE      5
#define BAT_BUFF_MIDDLE    2
#define BAT_LEVEL_K        100
#define BAT_LEVEL_OFFSET   0.67           //1N4148 fordward voltage (VDD)

// bluetooth commands (char)
#define BT_CMD_KEY_ON      '1'
#define BT_CMD_KEY_OFF     '0'
#define BT_CMD_INIT_CHAR   '#'
//#define BT_CMD_END_CHAR    0x0A
#define BT_CMD_END_CHAR    '$'

//input pins
#define PIN_KEY_STATUS     PIN_B0
#define PIN_RS232_RCV      PIN_B2
#define PIN_RS232_RCV_DBG  PIN_A1
#define PIN_BT_STATUS      PIN_B4   //corresponde al pin LED(24) del móludo bluetooth
#define PIN_BAT_LEVEL      PIN_A2
#define ANALOG_BAT_LEVEL   sAN2
#define ADC_CHANNEL        2

//outpus pins
#define PIN_RS232_XMIT     PIN_B5
#define PIN_RS232_XMIT_DBG PIN_A0      
#define PIN_BT_POWER       PIN_A3
#define PIN_BUZZER         PIN_A5

#use fixed_io (a_outputs=PIN_RS232_XMIT_DBG, PIN_BT_POWER, PIN_BUZZER) 
#use fixed_io (b_outputs=PIN_RS232_XMIT)
#use rs232(baud=9600,parity=N,xmit=PIN_RS232_XMIT,    rcv=PIN_RS232_RCV,    bits=8, stream=bluetooth)
#use rs232(baud=9600,parity=N,xmit=PIN_RS232_XMIT_DBG,rcv=PIN_RS232_RCV_DBG,bits=8, stream=debug)

//Includes
#include "C:\mplab\keyAdvisor\tonez.c"


enum {
   BT_OFF=10,
   BT_PAIRING,
   BT_READY,
   BT_WAITING_CMDOK,
   BT_CMD_RCV
}btState;


//funciones
void  init(void);
void  updateKeyStatus(void);
short isKeyOn(void);
void  setPortConfig(void);
void  setPortForSleepConfig(void);
void  setBtPower(short);
void  sendCommand(int8);
void  startTimeOut(void);
void  stopTimeOut(void);
short isTimeOut(void);
short isCmdRcvOk(void);
void  addBuffChar(char);
float getBatLevel();
void  addBatLevelVal(long);
void  startBatLevelMonitor();
void  stopBatLevelMonitor();


// define variables
static short   newKeyState;
static short   playSound;
static char    cmd;
static short   timeOut;
static int     timeOutCount;
static char    debugStep;
static char    sendBuff[BT_BUFF_SIZE];
static char    rcvBuff[BT_BUFF_SIZE];
static long    batLevelBuff[BAT_BUFF_SIZE];
static long    tempBuff[BAT_BUFF_SIZE];
static int     sendBuffPointer;
static int     rcvBuffPointer;
static int     batLevelBuffPointer;
static int     i;
static char    c;

