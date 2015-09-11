
//FlowMeter code version 1.0 - September 2015
//Please enjoy responsibly
//Added buzzer and decided that the core functionality was ready for v. 1.0. 

int brewPin = 3;            //Brew switch on pin 3 and gnd.
int pumpPin = 11;           //pump relay on pin 11 and gnd. Relay in 1
int valvePin = 12;          //valve relay on pin 12 and gnd. Relay in 2
int buzzPin = 8;            //Buzzer on pin 8
int held = 0;               //For how long has the button been engaged
int backF = 0 ;             //Number of backflush cycles counter
int stat = 0;               //Status from from the backflush to establish nominal or abort status
int timer = 0;              //For how many seconds the individual backFlush cycle should last. In milli seconds. See code.
int brewSwitch;             //Reading of brewSwitch
int flowMeter = 2;          //FlowMeter on pin 2, Digimesa
volatile int flowCount = 0; //Number of ticks from the flow meter. (1925 ticks per liter)
int flowCalib = 85 ;        //__--´´Calibrate here´´--__ ca. 2.3 tick/g
float startTime = 0.00;     //Brew timer start
float currentTime = 0.00;   //Brew time rigth now
float elapsedTime = 0.00;   //Brew time 
long maxBrewTime = 40000;   //max allowed brewTime: incl. infusiontime
 
                             
//Variables for frequncy measurements
volatile float tickFreq = 100.00; //Frequency of the flowmeter incoming tics.
int flushing = 200;               //If the tickFreq reached this number, we are flusing not brewing. Needs calibrating.
long timeOld = 0;                 //For measuring frequency.
int freqUpInt = 5;                //How often should the frequency be updated. Number of ticks. 
int oldFlowCount = 0;             //Stored flowcount value.
int freqChangeDetected = 0;       //For frequency change detection.
int freqLimit = 10;               //Frequency for puck resistance. Should also be calibrated.
long freqChangeTime = 0;          //The time the frequency changed happened

//Variables for pre-infusion
int preInCount = 0; 

// The following variables are long's because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
long time = 0;         // the last time the output pin was toggled
long debounce = 500;   // the debounce time, increase if the output flickers

void setup() {
  // Brew switch on pin 3 and gnd.
  pinMode(brewPin, INPUT_PULLUP);

  //Pump relay on pin 12 and gnd.
  pinMode(pumpPin, OUTPUT);

  //3-way valve on pin 11 and gnd.
  pinMode(valvePin, OUTPUT);

  //buzzer on pin 8 and gnd.
  pinMode(buzzPin, OUTPUT);

  //Make sure the relays are off.
  digitalWrite(pumpPin, HIGH);
  digitalWrite(valvePin, HIGH);

  //Flowmeter on input 2.
  pinMode(flowMeter, INPUT);
  
  //Makes sure that we count the ticks from the flow meter and the interrupt is attached.
  attachInterrupt(0, flowTics, RISING); 
  
  //Enable console output.
  Serial.begin(9600);
}

void loop() {
  
  //read brewswitch on Oscar
  brewSwitch = digitalRead(brewPin); 

  //Time the button engage time
  while (brewSwitch == LOW && millis() - time > debounce && held < 30)
  {
   //Sound the buzzer to notify user about heatFlush held time has been reached. 
   if( held >=10 && held <= 11){
    digitalWrite(buzzPin, HIGH);
   }
   else{
    digitalWrite(buzzPin, LOW);
   }
   delay(100);
   Serial.print("held ");
   Serial.println(held);
   brewSwitch = digitalRead(brewPin);
   held++;
  }
    //turn off the buzzer
    digitalWrite(buzzPin, LOW);
    
    if (held > 0 && held <= 10){
    //Start brewing, first Preinfusion, then brew
    Serial.println("BrewMode Enabled");

    //reset status 
    stat = 0;
    
    //reset button time
    time = millis(); 
    //Start brew timer
    startTime = millis();
    
    //run the preinfusion program
    stat = preInfuse();
    //digitalWrite(valvePin, LOW);

    if ( stat == 0) { //if stat is 0 the preinfuse process was completed and we can continue the brew process
      Serial.println("Pre-infusing ended");
      
      //Reset the ticks frequncy counter and the associated timer
      tickFreq = 0;
      timeOld = millis();
      oldFlowCount = 0;
      freqChangeDetected = 0; 
    
      //Turn on pump to start brewing
      Serial.println("Flowmeter controlled brew start");   
      digitalWrite(pumpPin, LOW);

      //Get new time
      currentTime = millis();
      elapsedTime = currentTime - startTime;

      //Keep pumping until that we have consistently low tics frequency (measure high and low frequencies and calibrate here).
      Serial.println("Detecting frequency change");
      while ( freqChangeDetected == 0 && elapsedTime < maxBrewTime){
    
        currentTime = millis();
        elapsedTime = currentTime - startTime;
        callBuzz(elapsedTime);
        if (tickFreq != 0 && millis() % 100 == 0){ 
          freqChangeDetected = detectFreqChange();
        }
        if ((int)elapsedTime % 100 == 0){
          Serial.print("            Time: ");
          Serial.print(elapsedTime/1000);
          Serial.println(" s");
        }
        
        brewSwitch = digitalRead(brewPin);
        if(brewSwitch == LOW && millis() - time > debounce )
        {
          flowCount = 1000000;
          freqChangeDetected = 1;
          Serial.println("--Brew Abort--");            
        }
        delay(1);
        callBuzz(elapsedTime);
      }

      //Only reset the counter if the brewing not have been aborted.
      if (flowCount < 1000000){
        flowCount=0;          
      }

      //Get new time
      currentTime = millis();
      elapsedTime = currentTime - startTime;     

        //Setting the change time will make sure that the buzzer is activated when the callBuzz function is called, 
        //to notify the brewer that a frequency change has been detected  
        freqChangeTime = elapsedTime;
        Serial.print("Detected at "); 
        Serial.print(freqChangeTime); 
        Serial.println(" ms");             
      //After a consistant change in frequency has been detected - start flowmeter controlled brewing.
      while (flowCount <= flowCalib && elapsedTime < maxBrewTime ){                
        brewSwitch = digitalRead(brewPin);
        currentTime = millis();
        elapsedTime = currentTime - startTime;
                     
        if(brewSwitch == LOW && millis() - time > debounce ){
            flowCount = 1000000;
            Serial.println("--Brew Abort--");            
        }         

        //print time
        if ((int)elapsedTime % 100 == 0){
          Serial.print("            Time: ");
          Serial.print(elapsedTime/1000);
          Serial.println(" s");
        }
        callBuzz(elapsedTime);
      }
      }
        
    //Turn off everything and reset
    currentTime = millis();
    turnOff();
    Serial.println("Brewing ended");
    Serial.print("Total shot time: ");
    Serial.print((currentTime - startTime)/1000);
    Serial.println(" s");
  }

  //HeatFlush
  if (held >= 11 && held < 30){
    Serial.println("HeatFlushMode Enabled");
    timer = 0;
    time = millis();//reset button time
    digitalWrite(pumpPin, LOW);
    digitalWrite(valvePin, LOW);
    while ( timer <= 3800) { //HeatFlush time
      brewSwitch = digitalRead(brewPin);  
      if (brewSwitch == LOW && millis() - time > debounce){
        //Abort
        timer = 10000;
        Serial.println("---HeatFlush Abort---");
      }      
      delay(1);
      timer++;
    }
    Serial.println("HeatFlushMode Ended");
    turnOff();
  }
  

  //BackFlush
  if (held == 30){
    Serial.println("BackFlushMode Enabled");
    time = millis();//reset button time 
    backF = 0;
    stat = 0;
    while (stat == 0 and backF <= 4){
      stat = backFlush();
      turnOff();
      if (stat == 0) {
        timer = 0;
      }
      Serial.println("BackFlush paused");
      while ( timer <= 10000 && stat == 0 ) { //Time the flush should be paused
        brewSwitch = digitalRead(brewPin);
        if (brewSwitch == LOW && millis() - time > debounce){
          Serial.println("---BackFlushMode Abort---");
          stat = 1;
          //Abort return
        }      
        delay(1);
        timer++;
      }
     backF++;
    }
    //When the back flusing is done, sound the horn to notify me a.k.a. the cleaner. 
    int i = 0;
    while ( i < 4){
      digitalWrite(buzzPin,HIGH);
      delay(300);
      digitalWrite(buzzPin,LOW);
      delay(300);
      i++;
    }
  Serial.println("BackFlushMode ended");
  }
}

  //Preinfuse function
  int preInfuse(){
    Serial.println("Pre-infusing start ");
    time = millis();//reset button time
    timer = 0;
    preInCount = 0;
    //Activate the 3-way valve to maintain pressure in grouphead. This will not be turned off until the turnOff() function is called. 
    digitalWrite(valvePin, LOW);
    while (preInCount < 4){
      Serial.print("Pre-infusing cycle: ");
      Serial.println(preInCount);       
      digitalWrite(pumpPin, LOW);
      while ( timer <= 1000) { //Time the pump should be activted in the preinfusion 
        brewSwitch = digitalRead(brewPin);
        if (brewSwitch == LOW && millis() - time > debounce){
          Serial.println("---PreInfusion Abort---");
          timer=10000;
          //Abort return
          preInCount = 1000;
        }      
        delay(1);
        timer++;
      }
      //pause for a predetemined time
      if (preInCount < 100 ) { 
        timer = 0; 
      }
      digitalWrite(pumpPin, HIGH);
      while ( timer <= 1000) { //Time the pump should be paused in the preinfusion
        brewSwitch = digitalRead(brewPin);
        if (brewSwitch == LOW && millis() - time > debounce){
          Serial.println("---PreInfusion Abort---");
          timer=10000;
          //Abort return
          preInCount = 1000;
        }      
        delay(1);
        timer++;
      }
             
      preInCount++;
      //reset the timer for the next run in the while loop
      timer = 0;
    }    
    if (preInCount == 1000) {
      return 1;    
    }
    else 
      return 0;
  }

  void callBuzz(int eTime){
    if (eTime >= 20000 && eTime <= 20300 || 
        eTime >= 25000 && eTime <= 25300 || 
        eTime >= 30000 && eTime <= 30300 ||
        eTime - freqChangeTime > 0 && eTime - freqChangeTime <= 150   ||
        eTime - freqChangeTime >= 250 && eTime - freqChangeTime <= 400){
      digitalWrite(buzzPin,HIGH);
    }
    else 
      digitalWrite(buzzPin,LOW);
  }
  
  //Turn off everything and reset
  void turnOff(){
      flowCount=0;
      digitalWrite(valvePin, HIGH);
      delay(200);
      digitalWrite(pumpPin, HIGH);
      brewSwitch == HIGH;
      held = 0;
      time = millis();//reset button time
      timer = 0;
      freqChangeTime = 0;
      digitalWrite(buzzPin,LOW);
  }

  //Backflush single cycle, return 0 for OK, 1 for abort
  int backFlush(){
    time = millis();//reset button time 
    digitalWrite(pumpPin, LOW);
    digitalWrite(valvePin, LOW);
    //If the button has been held for longer than 3 secs, start the pump with the intend to make the user remove the finger from the button.
    if (held == 30) {
      delay(2*debounce);
    }
    Serial.print("Cycle ");
    Serial.println(backF);
    timer = 0;
    while ( timer <= 10000) { //Time the flush should be engaged
      brewSwitch = digitalRead(brewPin);
      if (brewSwitch == LOW && millis() - time > debounce){
        Serial.println("---BackFlushMode Abort---");
        timer=20000;
        //Abort return
      }      
      delay(1);
      timer++;
    }
    if ( timer <=10001)
      return 0;    
    else 
      return 1;
  }

  //Function that the attached interupts call
  void flowTics() {
    Serial.print("Tick: ");
    Serial.println(flowCount);
    flowCount++;
    //get the frequency, in Hz, of the tics
    if (flowCount % freqUpInt == 0 ) {
      tickFreq = ((flowCount - oldFlowCount)*1000)/(millis() - timeOld);
      timeOld = millis();
      oldFlowCount = flowCount;
      Serial.print("                          Frequency: ");
      Serial.print(tickFreq,2);
      Serial.println(" Hz");
    } 
  }

  int detectFreqChange(){
    //if all variables are below the limit a frequency change has been detected
    if (tickFreq <= freqLimit ){
       freqChangeDetected = 1;       
       Serial.println("____-----    Frequency change detected    -----_____");
    }
    if  (freqChangeDetected == 1) 
      return 1;
    else
      return 0;
  }


  

