#include "C:\mplab\keyAdvisor\main.h"


/////////////////////////////////////////////////////
// EXTERNAL INTERRUPT: 
//   detects key modifications
//
/////////////////////////////////////////////////////
#int_EXT
void keyStatusChanged_isr(void) {      
   updateKeyStatus();
   switch(btState){             //por si diera la casualidad de que se cambia el estado justo despues de enviar el estado...
      case BT_WAITING_CMDOK:
      case BT_CMD_RCV:
         btState = BT_READY;
         break;
   }
}


/////////////////////////////////////////////////////
// RS232 INTERRUPT: 
//   captures commands from the bluetooth module (PC)
//
/////////////////////////////////////////////////////
#int_RDA
void  bluetoothCmdReceived_isr(void) {
   if(kbhit(bluetooth)){      
      c = fgetc(bluetooth);
      if (c!=0){
         switch(c){
            case BT_CMD_INIT_CHAR:
               rcvBuffPointer = 0;
               break;
            case BT_CMD_END_CHAR:        
               btState = BT_CMD_RCV;
               break;
         }          
         if (rcvBuffPointer >= BT_BUFF_SIZE-1){          //shit sometimes happends
            rcvBuffPointer=0;                  
         }
         rcvBuff[rcvBuffPointer++] = c;
      }
   }
}


//////////////////////////////////////////////////////////
// BATERY LEVEL INTERRUPT: 
//   report battery fuel gauge
//
//////////////////////////////////////////////////////////
#int_AD
void  AD_isr(void){       
   addBatLevelVal       (read_adc(ADC_READ_ONLY)); 
   disable_interrupts   (INT_AD);    
   clear_interrupt      (INT_RTCC);
   enable_interrupts    (INT_RTCC);
   set_timer0           (TIME_BAT_FUEL);
}
#int_RTCC
void  startADC_isr(void){
   disable_interrupts   (INT_RTCC);    
   clear_interrupt      (INT_AD);
   enable_interrupts    (INT_AD);    
   read_adc             (ADC_START_ONLY);
}


//////////////////////////////////////////////////////////
// TIMEOUT INTERRUPT: 
//   used to calculate pairing/commandOK timeout
//////////////////////////////////////////////////////////
#int_TIMER2
void timeOut_isr(){
   if(--timeOutCount == 0){
      timeOut = true;
   }
}


///////////////////////////////////////////////
// BLUETOOTH STATE MONITOR (PIN_BT_STATUS). 
//   - pairing:      led blink at 4-5Hz 
//   - connected:    led is ON (higth) 
///////////////////////////////////////////////
#int_RB
void  bluetoothModeChanged_isr(void){
   set_timer1(TIME_PAIRED);
   btState = BT_PAIRING;
   output_b(input_b());
}
#INT_TIMER1
void TIMER1_isr(void){
   if(input(PIN_BT_STATUS)){
      if(btState==BT_PAIRING){
         btState = BT_READY;
      }      
      
   } else {       //should never ocurr!!!
      setBtPower(OFF);   
   }
}


///////////////////////////////////////////////
// M A I N 
///////////////////////////////////////////////
void main(){   
   init();
   startBatLevelMonitor();
   
   if (debugStep!='a'){
      fprintf(debug, "init\r\n");
      debugStep='a';
   }   
   while(true){   
      if (newKeyState){
         if (playSound){
            playSound = false;
            if (isKeyOn()){
               generate_tone(C_NOTE[1],TIME_SHORT_NOTE);
               generate_tone(E_NOTE[1],TIME_SHORT_NOTE);
               generate_tone(G_NOTE[1],TIME_SHORT_NOTE);
               generate_tone(C_NOTE[2],TIME_LONG_NOTE);
            } else {
               generate_tone(C_NOTE[2],TIME_SHORT_NOTE);               
               generate_tone(G_NOTE[1],TIME_SHORT_NOTE);
               generate_tone(E_NOTE[1],TIME_SHORT_NOTE);
               generate_tone(C_NOTE[1],TIME_LONG_NOTE);               
            }
         }
      
         switch(btState){
            case BT_OFF: 
               if (debugStep!='b'){
                  fprintf(debug, "enabling bt\r\n");
                  debugStep='b';
               }   
               setBtPower(ON);               
               startTimeOut();
               breaK;                     
                        
            case BT_PAIRING:                 //wait for connection
               if (debugStep!='c'){
                  fprintf(debug, "conecting...\r\n");
                  debugStep='c';
               }   
               if (isTimeOut()){
                  stopTimeOut();
                  setBtPower(OFF);           //TODO: count btModule reboots to consider and sleep!!             
               }
               break;
               
            case BT_READY:
               if (isKeyOn()){
                  cmd = BT_CMD_KEY_ON;
               } else {
                  cmd = BT_CMD_KEY_OFF;               
               }
               
               sendCommand(cmd);
               btState = BT_WAITING_CMDOK;               
               break;
            
            case BT_WAITING_CMDOK:      
               if (debugStep!='e'){
                  fprintf(debug, "waiting response\r\n");
                  debugStep='e';
               }   
               if (isTimeOut()){
                  stopTimeOut();
                  btState = BT_READY;        //TODO: count errors to consider a btModule reboot!!             
               }
               break;
               
            case BT_CMD_RCV:               
               if(isCmdRcvOk()){
                  setBtPower(OFF);
                  newKeyState = false;
                  fprintf(debug, "Command OK\r\n");
               } else{
                  btState = BT_READY;
                  fprintf(debug, "Bad command!\r\n");
               }            
               break;
         }
   
      } else {
         if (debugStep!='g'){
            fprintf(debug, "sleep\r\n");
            debugStep='g';
         }   
         sleep();
         #asm NOP #endasm
      }
   }
}


///////////////////////////////////////////////
// KEY STATUS METHODS 
///////////////////////////////////////////////
void updateKeyStatus(void){
   if(isKeyOn()){
      ext_int_edge(INT_ONKEY_OFF);
      
   } else {
      ext_int_edge(INT_ONKEY_ON);
   }
   
   newKeyState = true;
   playSound   = true;
}
short isKeyOn(){
   return input(PIN_KEY_STATUS); 
}


//////////////////////////////////////////////////////
// SETUP METHODS
/////////////////////////////////////////////////////
void init(){
   setup_oscillator(OSC_INTRC|OSC_4MHZ);
   setup_adc(ADC_CLOCK_DIV_64);
   setup_adc_ports(ANALOG_BAT_LEVEL|VSS_VREF);
   set_adc_channel(ADC_CHANNEL);
   setup_spi(SPI_SS_DISABLED);
   setup_timer_0(RTCC_INTERNAL|T0_DIV_256);     //used to batery fuel gauge measurement  //TODO: set values!!!
   setup_timer_1(T1_INTERNAL|T1_DIV_BY_8);      //~500ms@4MHz. Control bluetooth pairing state                   
   setup_timer_2(T2_DIV_BY_16,0xC4,16);         //~50ms@4MHz. Used to check timeout (pairing, sending cmd, ...) 
   setup_comparator(NC_NC_NC_NC);
   setup_vref(FALSE);
   
   clear_interrupt(INT_AD);
   clear_interrupt(INT_EXT);
   clear_interrupt(INT_RDA);
   enable_interrupts(INT_AD);    //batery level
   enable_interrupts(INT_EXT);   //key state change detection
   enable_interrupts(INT_RDA);   //comunication with the bluetooth module   
         
   setBtPower(OFF);      
   updateKeyStatus();
   
   enable_interrupts(GLOBAL);   
}


//////////////////////////////////////////////////////
// BLUETOOTH METHODS
/////////////////////////////////////////////////////
void setBtPower(short state){   
   if(state){      
      output_high(PIN_BT_POWER);
      delay_ms(TIME_BT_ON);
      set_timer1(0);
      clear_interrupt(INT_RB);
      clear_interrupt(INT_TIMER1);
      clear_interrupt(INT_RDA);
      enable_interrupts(INT_RB);              
      enable_interrupts(INT_TIMER1);      
      enable_interrupts(INT_RDA);
      btState = BT_PAIRING;
      
   } else {
      disable_interrupts(INT_RB);   
      disable_interrupts(INT_TIMER1);
      disable_interrupts(INT_RDA);
      output_low(PIN_BT_POWER);
      btState = BT_OFF;
      stopTimeOut();
   }   
}  
void sendCommand(char cmd){  //TODO:mandar comando!!!!!!!!!!!!!!!!!!!!
   sendBuffPointer=0; 
   
   printf(addBuffChar,"%c%c%1.2fV%c", BT_CMD_INIT_CHAR,cmd,getBatLevel(),BT_CMD_END_CHAR);
   fprintf(bluetooth ,"%c%c%1.2fV%c", BT_CMD_INIT_CHAR,cmd,getBatLevel(),BT_CMD_END_CHAR);
   if (debugStep!='d'){
      fprintf(debug  ,"S:%c%c%1.2fV%c\r\n", BT_CMD_INIT_CHAR,cmd,getBatLevel(),BT_CMD_END_CHAR);
      debugStep='d';
   }
   startTimeOut();   
} 
short isCmdRcvOk(){
   fprintf(debug, "R:%c",rcvBuff[0]);
   if (rcvBuff[0]!=BT_CMD_INIT_CHAR) return false;   
   
   for (i=1;i<BT_BUFF_SIZE;i++){      
      fprintf(debug, "%c",rcvBuff[i]);
      if (rcvBuff[i]!=sendBuff[i]) return false;
      if (sendBuff[i]==BT_CMD_END_CHAR) break;
   }
   fprintf(debug, "%c\r\n",rcvBuff[i]);
   return true;
}
void addBuffChar(char c){
   sendBuff[sendBuffPointer++] = c;
}


///////////////////////////////////////////////
// TIMEOUT METHODS 
///////////////////////////////////////////////
void startTimeOut(){
   set_timer2(0);
   clear_interrupt(INT_TIMER2);           
   enable_interrupts(INT_TIMER2);
   timeOut        = false;
   timeOutCount   = TIMEOUT_COUNT;
}
void stopTimeOut(){
   disable_interrupts(INT_TIMER2);
}
short isTimeOut(){
   return timeOut;
}


///////////////////////////////////////////////
// BATERY LEVEL METHODS 
///////////////////////////////////////////////
void addBatLevelVal(long level){ 
   if (batLevelBuffPointer >= (BAT_BUFF_SIZE-1)){
      batLevelBuffPointer = 0;       
   }
   batLevelBuff[batLevelBuffPointer++] = level;
}
float getBatLevel(){
   long temp;
   int i, j;
   short flag = 1;
   
   //copy in a temporal buffer
   for (i=0;i<BAT_BUFF_SIZE;i++){
      tempBuff[i] = batLevelBuff[i];
   }      
      
   //bubble sort   
   for (i=0;i<BAT_BUFF_SIZE && flag;i++){
      flag=0;
      for(j=0;j<BAT_BUFF_SIZE-1;j++){
         if(tempBuff[j+1] > tempBuff[j]){
            temp = tempBuff[j];
            tempBuff[j] = tempBuff[j+1];
            tempBuff[j+1] = temp;
            flag = 1;
         }
      }         
   }   
      
   return ((float)tempBuff[BAT_BUFF_MIDDLE]/BAT_LEVEL_K) + BAT_LEVEL_OFFSET;
}
void startBatLevelMonitor(){   
   clear_interrupt(INT_RTCC);
   set_timer0(TIME_BAT_FUEL);   
   enable_interrupts(INT_RTCC);     //batery level   
}
void stopBatLevelMonitor(){
   disable_interrupts(INT_AD);      //batery level
   disable_interrupts(INT_RTCC);    //batery level
}
