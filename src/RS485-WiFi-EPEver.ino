/*
 *    RS485 TO  WIFI ADAPTOR CODE
 *    https://github.com/chickey/RS485-WiFi-EPEver
 *    by Colin Hickey 2021
 * 
 *    This code is designed to work with the specific board designed by meself which is on sale at tindie and my own website
 *    https://store.eplop.co.uk/product/epever-rs485-to-wifi-adaptor-new-revision/
 *    https://www.tindie.com/products/plop211/epever-rs485-to-wifi-adaptor-v15/
 * 
 *    3D printed case is available at https://www.thingiverse.com/thing:4766788/files
 *    
 *    If your just using just the code and would like to help out a coffee is always appreciated paypal.me/colinmhickey
 *    
 *    A big thankyou to the following project for getting me on the right path https://github.com/glitterkitty/EpEverSolarMonitor 
 *    I also couldn't have made this without the ESPUI project.
 *    
 *    Version 0.51
 *    
*/

#include <DNSServer.h>
#include <ESPUI.h>
#include <ModbusMaster.h>
#include <ESP8266WiFi.h>
//#include <SoftwareSerial.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>

#include <ESPAsyncWebServer.h>    //Local WebServer used to serve the configuration portal
#include <ESPAsyncWiFiManager.h>  // switched from tapzu to https://github.com/khoih-prog/ESPAsync_WiFiManager

#include <Updater.h>

const char* OTA_INDEX PROGMEM
    = R"=====(<!DOCTYPE html><html><head><meta charset=utf-8><title>OTA</title></head><body><div class="upload"><form method="POST" action="/ota" enctype="multipart/form-data"><input type="file" name="data" /><input type="submit" name="upload" value="Upload" title="Upload Files"></form></div></body></html>)=====";


////////////////
//#define DEBUG
//#define GUI_DEBUG
//#define MQTT_DEBUG
//#define INFLUX_DEBUG
////////////////

#include "settings.h"
#include "config.h"
#include "mqtt.h"
#include "influxdb.h"
#include "gui.h"

AsyncWebServer server(80);
DNSServer dns;
ModbusMaster node;   // instantiate ModbusMaster object


#ifndef TRANSMIT_PERIOD
  #define TRANSMIT_PERIOD 30000
#endif
unsigned long time_now = 0;

void setup(void) {
  //Attempt to read settings and if it fails resort to factory defaults
  if (!LoadConfigFromEEPROM())
    FactoryResetSettings();
  
  //  Create ESPUI interface tabs
  uint16_t tab1 = ESPUI.addControl( ControlType::Tab, "Settings 1", "Live Data" );
  uint16_t tab2 = ESPUI.addControl( ControlType::Tab, "Settings 2", "Historical Data" );
  uint16_t tab3 = ESPUI.addControl( ControlType::Tab, "Settings 3", "Settings" );
  uint16_t tab4 = ESPUI.addControl( ControlType::Tab, "Settings 4", "About" );

  //  Add Live Data controls
  SolarVoltage = ESPUI.addControl( ControlType::Label, "Solar Voltage", "0", ControlColor::Emerald, tab1);
  SolarAmps = ESPUI.addControl( ControlType::Label, "Solar Amps", "0", ControlColor::Emerald, tab1);
  SolarWattage = ESPUI.addControl( ControlType::Label, "Solar Wattage", "0", ControlColor::Emerald, tab1);
  BatteryVoltage = ESPUI.addControl( ControlType::Label, "Battery Voltage", "0", ControlColor::Emerald, tab1);
  BatteryAmps = ESPUI.addControl( ControlType::Label, "Battery Amps", "0", ControlColor::Emerald, tab1);
  BatteryWattage = ESPUI.addControl( ControlType::Label, "Battery Wattage", "0", ControlColor::Emerald, tab1);
  LoadVoltage = ESPUI.addControl( ControlType::Label, "Load Voltage", "0", ControlColor::Emerald, tab1);
  LoadAmps = ESPUI.addControl( ControlType::Label, "Load Amps", "0", ControlColor::Emerald, tab1);
  LoadWattage = ESPUI.addControl( ControlType::Label, "Load Wattage", "0", ControlColor::Emerald, tab1);
  BatteryStateOC = ESPUI.addControl( ControlType::Label, "Battery SOC", "0", ControlColor::Emerald, tab1);
  ChargingStatus = ESPUI.addControl( ControlType::Label, "Charging Status", "0", ControlColor::Emerald, tab1);
  BatteryStatus = ESPUI.addControl( ControlType::Label, "Battery Status", "4", ControlColor::Emerald, tab1);
  BatteryTemp = ESPUI.addControl( ControlType::Label, "Battery temperature", "0", ControlColor::Emerald, tab1);
  LoadStatus = ESPUI.addControl( ControlType::Label, "Load Status", "Off", ControlColor::Emerald, tab1);
  DeviceTemp = ESPUI.addControl( ControlType::Label, "Device Temp", "0", ControlColor::Emerald, tab1);

  // Add Historical Data Controls
  Maxinputvolttoday = ESPUI.addControl( ControlType::Label, "Max input voltage today", "0", ControlColor::Emerald, tab2);
  Mininputvolttoday = ESPUI.addControl( ControlType::Label, "Min input voltage today", "0", ControlColor::Emerald, tab2);
  MaxBatteryvolttoday = ESPUI.addControl( ControlType::Label, "Max battery voltage today", "0", ControlColor::Emerald, tab2);
  MinBatteryvolttoday = ESPUI.addControl( ControlType::Label, "Min battery voltage today", "0", ControlColor::Emerald, tab2);
  ConsumedEnergyToday = ESPUI.addControl( ControlType::Label, "Consumed energy today", "0", ControlColor::Emerald, tab2);
  ConsumedEnergyMonth = ESPUI.addControl( ControlType::Label, "Consumed energy this month", "0", ControlColor::Emerald, tab2);
  ConsumedEngeryYear = ESPUI.addControl( ControlType::Label, "Consumed energy this year", "0", ControlColor::Emerald, tab2);
  TotalConsumedEnergy = ESPUI.addControl( ControlType::Label, "Total consumed energy", "0", ControlColor::Emerald, tab2);
  GeneratedEnergyToday = ESPUI.addControl( ControlType::Label, "Generated energy today", "0", ControlColor::Emerald, tab2);
  GeneratedEnergyMonth = ESPUI.addControl( ControlType::Label, "Generated energy this month", "0", ControlColor::Emerald, tab2);
  GeneratedEnergyYear = ESPUI.addControl( ControlType::Label, "Generated energy this year", "0", ControlColor::Emerald, tab2);
  TotalGeneratedEnergy = ESPUI.addControl( ControlType::Label, "Total generated energy", "0", ControlColor::Emerald, tab2);
  Co2Reduction = ESPUI.addControl( ControlType::Label, "Carbon dioxide reduction", "0", ControlColor::Emerald, tab2);
  
  // Add Local Settings controls
  INFLUXDBIP = ESPUI.addControl( ControlType::Text, "InfluxDB IP", DEFAULT_INFLUXDB_HOST, ControlColor::Emerald, tab3 ,&InfluxDBIPtxt);
  INFLUXDBPORT = ESPUI.addControl( ControlType::Text, "InfluxDB Port", String(DEFAULT_INFLUXDB_PORT), ControlColor::Emerald, tab3 ,&InfluxDBPorttxt);
  INFLUXDBDB = ESPUI.addControl( ControlType::Text, "InfluxDB Database", DEFAULT_INFLUXDB_DATABASE, ControlColor::Emerald, tab3 ,&InfluxDBtxt);
  INFLUXDBUSER = ESPUI.addControl( ControlType::Text, "InfluxDB Username", DEFAULT_INFLUXDB_USER, ControlColor::Emerald, tab3 ,&InfluxDBUsertxt);
  INFLUXDBPASS = ESPUI.addControl( ControlType::Text, "InfluxDB Password", DEFAULT_INFLUXDB_PASSWORD, ControlColor::Emerald, tab3 ,&InfluxDBPasstxt);
  INFLUXDBEN = ESPUI.addControl(ControlType::Switcher, "Enable InfluxDB", "", ControlColor::Alizarin,tab3, &InfluxDBEnSwitch);  

  MQTTIP = ESPUI.addControl( ControlType::Text, "MQTT IP", DEFAULT_MQTT_SERVER, ControlColor::Emerald, tab3 ,&MQTTIPtxt);
  MQTTPORT = ESPUI.addControl( ControlType::Text, "MQTT Port", String(DEFAULT_MQTT_PORT), ControlColor::Emerald, tab3 ,&MQTTPorttxt);
  MQTTUSER = ESPUI.addControl( ControlType::Text, "MQTT Username", DEFAULT_MQTT_USERNAME, ControlColor::Emerald, tab3 ,&MQTTUsertxt);
  MQTTPASS = ESPUI.addControl( ControlType::Text, "MQTT Password", DEFAULT_MQTT_PASSWORD, ControlColor::Emerald, tab3 ,&MQTTPasstxt);
  MQTTTOPIC = ESPUI.addControl( ControlType::Text, "MQTT Topic", DEFAULT_MQTT_TOPIC, ControlColor::Emerald, tab3 ,&MQTTTopictxt);
  MQTTEN = ESPUI.addControl(ControlType::Switcher, "Enable MQTT", "", ControlColor::Alizarin,tab3, &MQTTEnSwitch);
  MQTT_HA_EN = ESPUI.addControl(ControlType::Switcher, "Enable HA Discovery", "", ControlColor::Alizarin,tab3, &MQTT_HAEnSwitch);

  DEVICEID = ESPUI.addControl( ControlType::Text, "Device ID", String(DEFAULT_DEVICE_ID), ControlColor::Emerald, tab3 ,&DEVICEIDtxt);
  DEVICEBAUD = ESPUI.addControl( ControlType::Text, "BAUD Rate", String(DEFAULT_SERIAL_BAUD), ControlColor::Emerald, tab3 ,&DEVICEBAUDtxt);
  
  LoadSwitchstate = ESPUI.addControl(ControlType::Switcher, "Load", "", ControlColor::Alizarin,tab3, &LoadSwitch);

  RebootButton = ESPUI.addControl( ControlType::Button, "Reboot", "Reboot", ControlColor::Peterriver, tab3, &RebootButtontxt );
  SaveButton = ESPUI.addControl( ControlType::Button, "Save Settings", "Save", ControlColor::Peterriver, tab3, &SaveButtontxt );
  savestatustxt = ESPUI.addControl( ControlType::Label, "Status:", "Changes Saved", ControlColor::Turquoise, tab3 );

  Abouttxt = ESPUI.addControl( ControlType::Label, "", "RS485 TO  WIFI ADAPTOR CODE<br><br>https://github.com/chickey/RS485-WiFi-EPEver<br><br>by Colin Hickey 2021<br><br>I'm always up for suggestions either via github or if you wish to chat with like minded people and pick people's brains on their setups i have setup a discord server<br><br>https://discord.gg/kBDmrzE", ControlColor::Turquoise,tab4 );
#ifdef ENABLE_HA_FACTORY_RESET_FUNCTIONS
  FactoryResetButton = ESPUI.addControl( ControlType::Button, "Reset to Factory Settings", "Reset", ControlColor::Alizarin, tab4, &FactoryResetButtontxt );
#endif

  Serial.begin(myConfig.Device_BAUD);

  // Connect D0 to RST to wake up
  pinMode(D0, WAKEUP_PULLUP);

  // init modbus in receive mode
  pinMode(MAX485_RE, OUTPUT);
  pinMode(MAX485_DE, OUTPUT);
  postTransmission();

  // EPEver Device ID and Baud Rate
  node.begin(myConfig.Device_ID, Serial);
    
  // modbus callbacks
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  AsyncWiFiManager wifiManager(&server,&dns);
  wifiManager.autoConnect("RS485-WiFi");
  wifiManager.setConfigPortalTimeout(180);
  ESPUI.jsonInitialDocumentSize = 16000; // This is the default, adjust when you have too many widgets or options
  setupGUI();  //Start Web Interface with OTA enabled

  uint8_t baseMac[6];
  WiFi.macAddress(baseMac);
  sprintf(baseMacChr, "%02X%02X%02X%02X%02X%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);

  setPublishHADiscovery = myConfig.HADiscovery_Enable;

}

void niceDelay(unsigned long delayTime)
{  
  if (myConfig.MQTT_Enable) {
    // establish/keep mqtt connection  
      mqtt_reconnect();
      mqtt_client.loop();
  }

  unsigned long startTime = millis();
  while (millis() - startTime < delayTime)
  {
    yield();
  }
}

uint16_t ReadRegister(uint16_t Register) {
  // Read register at the address passed in
  uint16_t RegisterValue;

  //niceDelay(50);
  node.clearResponseBuffer();
  uint8_t result = node.readInputRegisters(Register, 1);
  if (result == node.ku8MBSuccess)  {
    
    uint16_t RegisterValue = node.getResponseBuffer(0);

#ifdef DEBUG
    Serial.println(String(node.getResponseBuffer(0)));
#endif
  } else {
#ifdef DEBUG
    Serial.print(F("Miss read - ")); 
    Serial.print(Register);
    Serial.print(F(", ret val:"));
    Serial.println(result, HEX);
#endif
  }
  return RegisterValue;
}

void ReadValues() {  
  // clear old data
  //
  memset(rtc.buf,0,sizeof(rtc.buf));
  memset(live.buf,0,sizeof(live.buf));
  memset(stats.buf,0,sizeof(stats.buf));
  uint16_t result;
/*
  // Read registers for clock
  //
  niceDelay(50);
  node.clearResponseBuffer();
  uint8_t result = node.readHoldingRegisters(RTC_CLOCK, RTC_CLOCK_CNT);
  if (result == node.ku8MBSuccess)  {

    rtc.buf[0]  = node.getResponseBuffer(0);
    rtc.buf[1]  = node.getResponseBuffer(1);
    rtc.buf[2]  = node.getResponseBuffer(2);
    
  } else {
#ifdef DEBUG
    Serial.print(F("Miss read rtc-data, ret val:"));
    Serial.println(result, HEX);
#endif
  } 
  if (result==226)     ErrorCounter++;
  */
  // read LIVE-Data
  // 
  /*niceDelay(50);
  node.clearResponseBuffer();
  result = node.readInputRegisters(LIVE_DATA, LIVE_DATA_CNT);

  if (result == node.ku8MBSuccess)  {

    for(uint8_t i=0; i< LIVE_DATA_CNT ;i++) live.buf[i] = node.getResponseBuffer(i);
       
  } else {
#ifdef DEBUG
    Serial.print(F("Miss read liva-data, ret val:"));
    Serial.println(result, HEX);
#endif
  }

  // Statistical Data
  niceDelay(50);
  node.clearResponseBuffer();
  result = node.readInputRegisters(STATISTICS, STATISTICS_CNT);

  if (result == node.ku8MBSuccess)  {
    
    for(uint8_t i=0; i< STATISTICS_CNT ;i++)  stats.buf[i] = node.getResponseBuffer(i);
    
  } else {
#ifdef DEBUG
    Serial.print(F("Miss read statistics, ret val:"));
    Serial.println(result, HEX);
#endif
  } */

  //New addition
  // Panel voltage
  solvolt = ReadRegister(PANEL_VOLTS);
  BatteryType = ReadRegister(BATTERY_TYPE);
  EQChargeVoltValue = ReadRegister(EQ_CHARGE_VOLT);
  ChargeLimitVolt = ReadRegister(CHARGING_LIMIT_VOLT);
  BatteryCapactity = ReadRegister(BATTERY_CAPACITY);
  batterySOC = ReadRegister(BATTERY_SOC);
  loadState = ReadRegister(LOAD_STATE);
  CCModel = ReadRegister(CCMODEL);

/*  

  // Battery Net Current = Icharge - Iload
  niceDelay(50);
  node.clearResponseBuffer();
  result = node.readInputRegisters(  BATTERY_CURRENT_L, 2);
  if (result == node.ku8MBSuccess)  {
    
    batteryCurrent = node.getResponseBuffer(0);
    batteryCurrent |= node.getResponseBuffer(1) << 16;
    
  } else {
#ifdef DEBUG
    Serial.print(F("Miss read batteryCurrent, ret val:"));
    Serial.println(result, HEX);
#endif
  }
 
  if (!switch_load) {
    // State of the Load Switch
    niceDelay(50);
    node.clearResponseBuffer();
    result = node.readCoils(  LOAD_STATE, 1 );
    if (result == node.ku8MBSuccess)  {
      
      loadState = node.getResponseBuffer(0);
          
    } else  {
#ifdef DEBUG
      Serial.print(F("Miss read loadState, ret val:"));
      Serial.println(result, HEX);
 #endif
    }
  }
    
  // Read Status Flags
  niceDelay(50);
  node.clearResponseBuffer();
  result = node.readInputRegisters(  0x3200, 2 );
  if (result == node.ku8MBSuccess)  {

    uint16_t temp = node.getResponseBuffer(0);
#ifdef DEBUG 
    Serial.print(F("Batt Flags : "));
    Serial.println(temp);
#endif
    
    status_batt.volt = temp & 0b1111;
    status_batt.temp = (temp  >> 4 ) & 0b1111;
    status_batt.resistance = (temp  >>  8 ) & 0b1;
    status_batt.rated_volt = (temp  >> 15 ) & 0b1;
    
    temp = node.getResponseBuffer(1);
#ifdef DEBUG 
    Serial.print(F("Chrg Flags : "));
    Serial.println(temp, HEX); 
#endif

    charger_mode = ( temp & 0b0000000000001100 ) >> 2 ;
#ifdef DEBUG 
    Serial.print(F("charger_mode  : "));
    Serial.println( charger_mode );
#endif
  } else {
#ifdef DEBUG
    Serial.print(F("Miss read ChargeState, ret val:"));
    Serial.println(result, HEX);
#endif
  }

*/

}

void preTransmission()
{
  digitalWrite(MAX485_RE, 1);
  digitalWrite(MAX485_DE, 1);
}

void postTransmission()
{
  digitalWrite(MAX485_RE, 0);
  digitalWrite(MAX485_DE, 0);
}

void debug_output(){
#ifdef DEBUG
  //Output values to serial
  Serial.printf("\n\nTime:  20%02d-%02d-%02d   %02d:%02d:%02d   \n",  rtc.r.y , rtc.r.M , rtc.r.d , rtc.r.h , rtc.r.m , rtc.r.s  );
  
  Serial.print(  F("\nLive-Data:           Volt        Amp       Watt  "));
  Serial.printf( "\n  Panel:            %7.3f    %7.3f    %7.3f ",  solvolt/100.f ,  live.l.pI/100.f ,  live.l.pP/100.0f );
  Serial.printf( "\n  Batt:             %7.3f    %7.3f    %7.3f ",  live.l.bV/100.f ,  live.l.bI/100.f ,  live.l.bP/100.0f );
  Serial.printf( "\n  Load:             %7.3f    %7.3f    %7.3f \n",  live.l.lV/100.f ,  live.l.lI/100.f ,  live.l.lP/100.0f );
  Serial.printf( "\n  Battery Current:  %7.3f  A ",      batteryCurrent/100.f  );
  Serial.printf( "\n  Battery SOC:      %7.0f  %% ",     batterySOC/1.0f  );
  Serial.printf( "\n  Load Switch:          %s   ",     (loadState==1?" On":"Off") );

  Serial.print(  F("\n\nStatistics:  "));
  Serial.printf( "\n  Panel:       min: %7.3f   max: %7.3f   V", stats.s.pVmin/100.f  , stats.s.pVmax/100.f  );
  Serial.printf( "\n  Battery:     min: %7.3f   max: %7.3f   V\n", stats.s.bVmin /100.f , stats.s.bVmax/100.f);

  Serial.printf( "\n  Consumed:    day: %7.3f   mon: %7.3f   year: %7.3f  total: %7.3f   kWh",
      stats.s.consEnerDay/100.f  ,stats.s.consEnerMon/100.f  ,stats.s.consEnerYear/100.f  ,stats.s.consEnerTotal/100.f   );
  Serial.printf( "\n  Generated:   day: %7.3f   mon: %7.3f   year: %7.3f  total: %7.3f   kWh",
      stats.s.genEnerDay/100.f   ,stats.s.genEnerMon/100.f   ,stats.s.genEnerYear/100.f   ,stats.s.genEnerTotal/100.f  );
  Serial.printf( "\n  CO2-Reduction:    %7.3f  t\n",      stats.s.c02Reduction/100.f  );

  Serial.print(  F("\nStatus:"));
  Serial.printf( "\n    batt.volt:         %s   ",     batt_volt_status[status_batt.volt] );
  Serial.printf( "\n    batt.temp:         %s   ",     batt_temp_status[status_batt.temp] );
  Serial.printf( "\n    charger.charging:  %s   \n\n",     charger_charging_status[ charger_mode] );
#endif
}

void loop(void) {
  //check if we should publish the Home Assistant Discovery MQTT packets
  if (setPublishHADiscovery)
  {
    publishHADiscovery();
  }

  String buffer;
  buffer.reserve(64);
  // Print out to serial if debug is enabled.
  //
#ifdef DEBUG
    debug_output();
#endif

// Do these need to be continuously updated as they only change when the
//   user changes them via webpage
// Disabling them should reduce the JSON packet size from ESPUI
  ESPUI.updateControlValue(MQTTIP , String(myConfig.mqtt_server));
  ESPUI.updateControlValue(MQTTPORT , String(myConfig.mqtt_port));
  ESPUI.updateControlValue(MQTTUSER , String(myConfig.mqtt_username));
  ESPUI.updateControlValue(MQTTPASS , String(myConfig.mqtt_password));
  ESPUI.updateControlValue(MQTTTOPIC , String(myConfig.mqtt_topic));
  ESPUI.updateControlValue(MQTTEN , String(myConfig.MQTT_Enable));
  ESPUI.updateControlValue(MQTT_HA_EN , String(myConfig.HADiscovery_Enable));


  ESPUI.updateControlValue(INFLUXDBIP , String(myConfig.influxdb_host));
  ESPUI.updateControlValue(INFLUXDBPORT , String(myConfig.influxdb_httpPort));
  ESPUI.updateControlValue(INFLUXDBUSER , String(myConfig.influxdb_user));
  ESPUI.updateControlValue(INFLUXDBPASS , String(myConfig.influxdb_password));
  ESPUI.updateControlValue(INFLUXDBDB , String(myConfig.influxdb_database));
  ESPUI.updateControlValue(INFLUXDBEN , String(myConfig.influxdb_enabled));

  ESPUI.updateControlValue(DEVICEID , String(myConfig.Device_ID));
  ESPUI.updateControlValue(DEVICEBAUD , String(myConfig.Device_BAUD));

  // Read Values from Charge Controller
  ReadValues();

  //Update ESPUI Live Data components
  ESPUI.updateControlValue(LoadStatus , String(loadState==1?F(" On"):F("Off")));
  ESPUI.updateControlValue(DeviceTemp , String(batt_temp_status[status_batt.temp]));
  
  buffer.concat(solvolt/100.f); buffer.concat(F("V"));
  ESPUI.updateControlValue(SolarVoltage , buffer);

  buffer.clear(); buffer.concat(live.l.pI/100.f); buffer.concat(F("A"));
  ESPUI.updateControlValue(SolarAmps , buffer);
  
  buffer.clear(); buffer.concat(live.l.pP/100.0f); buffer.concat(F("w"));
  ESPUI.updateControlValue(SolarWattage , buffer);
  
  buffer.clear(); buffer.concat(solvolt/100.f); buffer.concat(F("V"));
  ESPUI.updateControlValue(BatteryVoltage  , buffer);
  
  buffer.clear(); buffer.concat(live.l.bI/100.f); buffer.concat(F("A"));
  ESPUI.updateControlValue(BatteryAmps , buffer);
  
  buffer.clear(); buffer.concat(live.l.bP/100.0f); buffer.concat(F("w"));
  ESPUI.updateControlValue(BatteryWattage , buffer);
  
  buffer.clear(); buffer.concat(live.l.lV/100.f); buffer.concat(F("V"));
  ESPUI.updateControlValue(LoadVoltage , buffer);
  
  buffer.clear(); buffer.concat(live.l.lI/100.f); buffer.concat(F("A"));
  ESPUI.updateControlValue(LoadAmps , buffer);
  
  buffer.clear(); buffer.concat(live.l.lP/100.0f); buffer.concat(F("w"));
  ESPUI.updateControlValue(LoadWattage , buffer);
  
  buffer.clear(); buffer.concat(batterySOC/1.0f); buffer.concat(F("%"));
  ESPUI.updateControlValue(BatteryStateOC , buffer);
  ESPUI.updateControlValue(ChargingStatus , String(charger_charging_status[ charger_mode]));
  ESPUI.updateControlValue(BatteryStatus , String(batt_volt_status[status_batt.volt]));
  ESPUI.updateControlValue(BatteryTemp , String(batt_temp_status[status_batt.temp]));

  //Update historical values
  buffer.clear(); buffer.concat(stats.s.pVmax/100.f); buffer.concat(F("V"));
  ESPUI.updateControlValue(Maxinputvolttoday, buffer);
  
  buffer.clear(); buffer.concat(stats.s.pVmin/100.f); buffer.concat(F("V"));
  ESPUI.updateControlValue(Mininputvolttoday , buffer);
  
  buffer.clear(); buffer.concat(stats.s.bVmax/100.f); buffer.concat(F("V"));
  ESPUI.updateControlValue(MaxBatteryvolttoday , buffer);
  
  buffer.clear(); buffer.concat(stats.s.bVmin/100.f); buffer.concat(F("V"));
  ESPUI.updateControlValue(MinBatteryvolttoday, buffer);
  
  buffer.clear(); buffer.concat(stats.s.consEnerDay/100.f); buffer.concat(F("kWh"));
  ESPUI.updateControlValue(ConsumedEnergyToday , buffer);
  
  buffer.clear(); buffer.concat(stats.s.consEnerMon/100.f); buffer.concat(F("kWh"));
  ESPUI.updateControlValue(ConsumedEnergyMonth , buffer);
  
  buffer.clear(); buffer.concat(stats.s.consEnerYear/100.f); buffer.concat(F("kWh"));
  ESPUI.updateControlValue(ConsumedEngeryYear , buffer);
  
  buffer.clear(); buffer.concat(stats.s.consEnerTotal/100.f); buffer.concat(F("kWh"));
  ESPUI.updateControlValue(TotalConsumedEnergy , buffer);
  
  buffer.clear(); buffer.concat(stats.s.genEnerDay/100.f); buffer.concat(F("kWh"));
  ESPUI.updateControlValue(GeneratedEnergyToday , buffer);
  
  buffer.clear(); buffer.concat(stats.s.genEnerMon/100.f); buffer.concat(F("kWh"));
  ESPUI.updateControlValue(GeneratedEnergyMonth , buffer);
  
  buffer.clear(); buffer.concat(stats.s.genEnerYear/100.f); buffer.concat(F("kWh"));
  ESPUI.updateControlValue(GeneratedEnergyYear , buffer);
  
  buffer.clear(); buffer.concat(stats.s.genEnerTotal/100.f); buffer.concat(F("kWh"));
  ESPUI.updateControlValue(TotalGeneratedEnergy , buffer);
  
  buffer.clear(); buffer.concat(stats.s.c02Reduction/100.f); buffer.concat(F("t"));
  ESPUI.updateControlValue(Co2Reduction , buffer);
  
  // Do the Switching of the Load here and post new state to MQTT if enabled
  if( switch_load == 1 ){
    switch_load = 0;  

#ifdef DEBUG
    Serial.print(F("Switching Load "));
    Serial.println( (loadState?F("On"):F("Off")) );
    niceDelay(200);
#endif

    node.writeSingleCoil(0x0002, loadState);

#ifdef DEBUG
    /*if (result != node.ku8MBSuccess)  {
      Serial.print(F("Miss write loadState, ret val:"));
      Serial.println(result, HEX);
    }*/
#endif

    //reset the transmission timer to avoid publishing twice
    time_now += TRANSMIT_PERIOD;
    if (myConfig.MQTT_Enable) {
      // establish/keep mqtt connection
      mqtt_reconnect();
      mqtt_publish();
      mqtt_client.loop();
    }

    // establish/keep influxdb connection
    if(myConfig.influxdb_enabled == 1) {
      Influxdb_postData(); 
    }
  }
  
  // Transmit to MQTT/Influx if timer has elapsed.
  if(millis() >= time_now + TRANSMIT_PERIOD){
    time_now += TRANSMIT_PERIOD;

    if (myConfig.MQTT_Enable) {
        // establish/keep mqtt connection  
        mqtt_reconnect();
        mqtt_publish();
        mqtt_client.loop();
    }

    // establish/keep influxdb connection
    if(myConfig.influxdb_enabled == 1) {
      Influxdb_postData(); 
    }
  }

  //Check error count and if it exceeds 5 reset modbus
#ifdef DEBUG
  Serial.print(F("Error count = "));
  Serial.println(ErrorCounter);
#endif
  if (ErrorCounter>5) {
    // init modbus in receive mode
    pinMode(MAX485_RE, OUTPUT);
    pinMode(MAX485_DE, OUTPUT);
    postTransmission();

    // EPEver Device ID and Baud Rate
    node.begin(myConfig.Device_ID, Serial);
    
    // modbus callbacks
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);
    ErrorCounter = 0;  
  }
  
  // power down MAX485_DE
  postTransmission();
}
