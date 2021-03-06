/*
  ArcolaEnergy h2mdk fuelcell controller for 3, 12 and 30W stacks.
  http://www.arcolaenergy.com/h2mdk

  Copyright (C) 2012  ArcolaEnergy

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.


  TODO:
  * add 3w overload protection
*/

#ifndef h2mdk_h
#define h2mdk_h

#if ARDUINO >= 100
  #include <Arduino.h>
#else
  #include <WProgram.h>
#endif

//stack version types
#define V1_5W 1
#define V3W 2
#define V12W 3
#define V30W 4
//hardware version types
#define V1_0 1
#define V1_2 2
#define V1_3 3

class h2mdk
{
  public:
    void poll();
    void status();
    float getVoltage();
    float getCurrent();
    void start();
    void overrideTimings( unsigned int, unsigned int, unsigned long, unsigned int );
    void enablePurge();
    void disablePurge();
    void enableShort();
    void disableShort();
  private:
    int _vccRead();
    bool _ledstate;
    bool _ni(bool);
    float _current;
    float _voltage;
    float _capVoltage;
    void _shortCircuit();
    void _printTimings();
    void _purge();
    void _updateElect();
    void _blink();
    void _checkCaps();
    void _initializeTimings();
    bool _doShort;
    bool _doPurge;

//timing defs
    static const int PREPURGE = 100; //to ensure we don't purge while shorting - doesn't happen with default timings.
    static const int ELECT_INTERVAL = 400; //how often to sample electrical
    static const int BLINK_INTERVAL = 500; //how often to sample electrical

    static const float FILTER = 0.9; //coefficient for LPF on current sense

//digital IO pins
  #if( _shield == V1_0 )
    static const int PURGE = 2;
    static const int LOAD = 3;  // 3w can disconnect load
    static const int OSC = 3;   // 12-30w needs oscillator for mosfet charge pump
    static const int SHORT = 4;
    static const int STATUS_LED = 5;
  #elif( _shield == V1_2 || _shield == V1_3)
    static const int PURGE = 3;
    static const int LOAD = 4;  // 3w can disconnect load
    static const int SHORT = 5;
    static const int STATUS_LED = 6;
  #endif
//analog pins
    static const int VOLTAGE_SENSE = A1;
    static const int CURRENT_SENSE = A2;
    static const int CAP_V_SENSE = A3;

    static const int CAP_V = 3500; //mv
  #if( _shield == V1_0 )
      static const int AREF = 5000;
      static const float capDivider = 1.0;
    #if( _stacksize == V1_5W || _stacksize == V3W )
      static const float FCDivider = 1.0;
    #else
      static const float FCDivider = 326.00/100.00; // R1=226k R2=100k1.0;
    #endif
  #elif(_shield == V1_2 || _shield == V1_3 )
      static const int AREF = 3300;
      static const float capDivider = 2;
    #if( _stacksize == V1_5W || _stacksize == V3W )
      static const float FCDivider = 2; //untested
    #else
      static const float FCDivider = 97.00/22.00; // R1=75k R2=22
    #endif
  #endif
      

//for doing the timing
    unsigned int _shortCircuitTimer;
    unsigned int _electTimer;
    unsigned long _purgeTimer;
    unsigned int _statusTimer;
    unsigned long _lastPoll;
    float _filteredRawCurrent;

    unsigned int _shortCircuitInterval;
    unsigned int _shortTime;
    unsigned long _purgeInterval;
    unsigned int _purgeTime;
};

void h2mdk::overrideTimings( unsigned int sci, unsigned int st, unsigned long pi, unsigned int pt )
{
    Serial.println( "override timings...");
    _shortCircuitInterval = sci;
    _shortTime = st;
    _purgeInterval = pi;
    _purgeTime = pt;
    _printTimings();
}

void h2mdk::_initializeTimings()
{
/*
1.5W stack (info from Horizon)
Purge: 100ms every 4 mins
Short circuit: 100ms every 10s

H-12 (no info from Horizon as they don't have H-12 controller)
Purge: Shall we assume 50ms every 25s (because H2 consumption is less)
Short circuit: 100ms every 10s (assuming same as other stacks)

H-30 (info from Horizon)
Purge: 50ms every 10s
Short circuit: 100ms every 10s
*/
//time vars
  // all in ms
  #if( _stacksize == V1_5W )
    _shortCircuitInterval = 10000;
    _shortTime = 100;
    _purgeInterval = 60000; //240000;
    _purgeTime = 100;

  #elif( _stacksize == V3W )
    _shortCircuitInterval = 10000;
    _shortTime = 100;
    _purgeInterval = 60000; //240000;
    _purgeTime = 100;

  #elif( _stacksize == V12W )
    _shortCircuitInterval = 10000;
    _shortTime = 100;
    _purgeInterval = 25000;
    _purgeTime = 100;

  #elif( _stacksize == V30W )
    _shortCircuitInterval = 10000;
    _shortTime = 100;
    _purgeInterval = 10000;
    _purgeTime = 100;
  #else
  #endif
}
void h2mdk::start()
{
  _shortCircuitTimer = 5000; //start off out of phase with purge
  _purgeTimer = 0;
  _electTimer = 0;
  _statusTimer = 0;
  
  //enable purge and short circuit by default
  _doShort = true;
  _doPurge = true; 
  
  //setup default timings
  _initializeTimings();

  //initialize the current so that when we startup it isn't negative
  _filteredRawCurrent = 1024.0/AREF*2500.0; //should be at zero A (ie 2500mv) to start with

  //for the latest hardware, aref is connected to the Arduino's 3.3v output to get a more stable reference for the ADC
  #if( _shield == V1_2 || _shield == V1_3 )
    analogReference(EXTERNAL);
  #endif

  //pin def stuff
  pinMode(STATUS_LED, OUTPUT);

  if( _stacksize == V3W || _stacksize == V1_5W )
  {
    pinMode( LOAD, OUTPUT );
    //connect the load to charge the caps
    digitalWrite( LOAD, _ni(HIGH) );
  }
  
  #if( _shield == V1_0 && (_stacksize == V12W || _stacksize == V30W ))
    //charge pump waveform
    analogWrite( OSC, 128 );
  #endif

  pinMode( SHORT, OUTPUT );
  digitalWrite( SHORT, _ni(LOW) );
  pinMode( PURGE, OUTPUT );
  digitalWrite( PURGE, _ni(LOW) );

  //show the user what version we are
  Serial.println( "ArcolaEnergy fuel cell controller for "
  #if( _stacksize == V1_5W )
    "1.5W"
  #elif( _stacksize == V3W )
    "3W"
  #elif( _stacksize == V12W )
    "12W"
  #elif( _stacksize == V30W )
    "30W"
  #else
    "unknown!"
  #endif
  );

  Serial.println( "Hardware version "
  #if( _shield == V1_0 ) //first prototypes
    "v1.0"
  #elif( _shield == V1_2 ) //production run for 1.5/3w
    "v1.2"
  #elif( _shield == V1_3 ) //production run for 12/30w
    "v1.3"
  #else
    "unknown!"
   #endif
  );

  _printTimings();

  //wait for cap to charge
  Serial.println( "waiting for caps to charge" );
  _checkCaps();
}

//allow user to control whether we are shorting/purging
void h2mdk::disableShort()
{
    _doShort = false;
}
void h2mdk::enableShort()
{
    _doShort = true;
}
void h2mdk::disablePurge()
{
    _doPurge = false;
}
void h2mdk::enablePurge()
{
    _doPurge = true;
}

float h2mdk::getVoltage()
{
  return _voltage;
}

float h2mdk::getCurrent()
{
  return _current;
}

void h2mdk::status()
{
  Serial.print( _voltage );
  Serial.print( "V, " );
  Serial.print( _current );
  Serial.println( "A" );
}

//deals with all the timing. Should be called about every 100ms
void h2mdk::poll()
{
  int interval = millis() - _lastPoll;
  _lastPoll = millis();

  _electTimer += interval;
  _statusTimer += interval;
  _purgeTimer += interval;
  _shortCircuitTimer += interval;

  _lastPoll = millis();

  if( _statusTimer > BLINK_INTERVAL )
  {
    _blink();
    status();
    _statusTimer = 0;
  }
  if( _electTimer > ELECT_INTERVAL )
  {
    _updateElect();
    _electTimer = 0;
  }
  if( _shortCircuitTimer > _shortCircuitInterval )
  {
    _shortCircuit();
    _shortCircuitTimer = 0;
  }
  if( _purgeTimer > _purgeInterval )
  {
    _purge();
    _purgeTimer = 0;
  }

}

//block until capacitors are charged
void h2mdk::_checkCaps()
{

  while( _capVoltage < CAP_V)
  {
    _updateElect();
    Serial.print("cap: ");
    Serial.print(_capVoltage);
    Serial.println("mv");
    _blink();
    delay(200);
  }
  Serial.println( "CHARGED" );
}

//update all the electrical measurements
void h2mdk::_updateElect()
{
  //cap voltage
  _capVoltage = capDivider*AREF/1024.0*analogRead(CAP_V_SENSE ) ;

  //stackvoltage
  float rawStackV = analogRead(VOLTAGE_SENSE );
  float stackVoltage;

  stackVoltage = FCDivider * (AREF/1024.0*rawStackV); 
  _voltage = stackVoltage/1000;

  //current
  //100 times average of current.
  _filteredRawCurrent = _filteredRawCurrent * FILTER  + ( 1 - FILTER ) * analogRead(CURRENT_SENSE);
  float currentMV = (AREF/ 1024.0 ) * _filteredRawCurrent;
  if( _stacksize == V3W || _stacksize == V1_5W )
    //current sense chip is powered from 5v regulator
    _current = ( currentMV - 5020 / 2 ) / 185; //185mv per amp
  else if( _stacksize == V12W || _stacksize == V30W )
    //current sense chip is powered by arduino supply
    _current = ( currentMV - 5000 / 2 ) / 185; //185mv per amp
	
}

//purge the waste gas in the stack
void h2mdk::_purge()
{
  if( _doPurge == false )
  {
    Serial.println("USER REQUESTS SKIPPING PURGE");
    return;
  }
  Serial.println("PURGE");
  
  if( _stacksize == V3W || _stacksize == V1_5W )
    //disconnect load
    digitalWrite( LOAD, _ni(LOW) );

  digitalWrite( PURGE, _ni(HIGH) );
  delay( _purgeTime);
  digitalWrite( PURGE, _ni(LOW) );

  if( _stacksize == V3W || _stacksize == V1_5W )
    //disconnect load
    digitalWrite( LOAD, _ni(HIGH) );
}

//short circuit the stack to keep the temperature right
void h2mdk::_shortCircuit()
{
  if( _doShort == false )
  {
    Serial.println("USER REQUESTS SKIPPING SHORT-CIRCUIT");
    return;
  }
  if( _capVoltage < CAP_V)
  {
    Serial.println("SKIPPING SHORT-CIRCUIT AS SUPERCAP VOLTAGE TOO LOW");
    return;
  }
  Serial.println("SHORT-CIRCUIT");

  //disconnect load if we can
  if( _stacksize == V3W || _stacksize == V1_5W )
    digitalWrite( LOAD, _ni(LOW) );

  //short circuit
  digitalWrite( SHORT, _ni(HIGH) );
  delay(_shortTime);
  digitalWrite( SHORT, _ni(LOW) );

  //reconnect load if we can
  if( _stacksize == V3W || _stacksize == V1_5W )
    digitalWrite( LOAD, _ni(HIGH) );
}

//utility function to show the timings that have been set
void h2mdk::_printTimings()
{
  Serial.print("Short-circuit: ");
  Serial.print(_shortTime);
  Serial.print("ms every ");
  Serial.print(_shortCircuitInterval / 1000);
  Serial.println(" s");

  Serial.print("Purge: ");
  Serial.print(_purgeTime);
  Serial.print("ms every ");
  Serial.print( _purgeInterval / 1000);
  Serial.println(" s");

    //double blink to show we've started
  _blink(); delay(100); _blink(); delay(100); _blink(); delay(100);

  //wait for cap to charge
  Serial.println( "waiting for caps to charge" );
  _checkCaps();
}



//utility to invert the mosfet pins for the older version 12 and 30W control boards
inline bool h2mdk::_ni(bool state)
{
  if( _stacksize == V3W || _stacksize == V1_5W )
    return state;
  if( _stacksize == V12W || _stacksize == V30W )
  {
    if( _shield == V1_0 )
      return ! state;
    else if (_shield == V1_3 )
      return state;
  }
}

//blink the status led
void h2mdk::_blink()
{
  digitalWrite(STATUS_LED, _ledstate );
  _ledstate = ! _ledstate;
}

#endif
