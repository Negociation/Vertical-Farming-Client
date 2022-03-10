// PocketFarm Client System Controller
// Versão 0.1
// Autor: Marcos T. 
// Email: tiagitofranca04@gmail.com

#include "RTClib.h"
#include "Adafruit_NeoPixel.h"
#include "StackArray.h"
#include "SD.h"
#include "ArduinoUniqueID.h"
#include "ArduinoJson.h"



#include "SPI.h"
#include "Wire.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "Adafruit_Sensor.h"
#include "Adafruit_BME280.h"

//Definições para o Modo Legado (Offline)
const String def_cpuConfig = "{ \"cpuPresencePin\":46,\"cpuPresenceInterval\":-1, \"cpuLevels\":[ { \"levelId\":1001, \"levelLightStatus\":1, \"levelLightPins\":[ 44, 45 ], \"levelLights\":{ \"mode\":\"0\", \"scheme\":[ ] }, \"levelCarbonPin\":\"A10\", \"levelTemperaturePin\":47, \"levelMoisturePins\":[ \"A13\", 34, 35 ] }, { \"levelId\":2, \"levelLightStatus\":1, \"levelLightPins\":[ 42, 43 ], \"levelLights\":{ \"mode\":\"0\", \"scheme\":[ ] }, \"levelCarbonPin\":\"A11\", \"levelTemperaturePin\":48, \"levelMoisturePins\":[ \"A14\", 36, 37 ] }, { \"levelId\":3, \"levelLightStatus\":1, \"levelLightPins\":[ 40, 41 ], \"levelLights\":{ \"mode\":\"0\", \"scheme\":[ ] }, \"levelCarbonPin\":\"A12\", \"levelTemperaturePin\":49, \"levelMoisturePins\":[ \"A15\", 38, 39 ] } ] }";

//Read Serial Inputs
String serialRead(){
  String sdata;
  byte ch;

  if (Serial.available()){
      ch = Serial.read();
      sdata += (char)ch;
      if (ch=='\n') {  // Command recevied and ready.
         sdata.trim();
      }
   }
   return sdata;
}


DynamicJsonDocument jsonRead(String jsonInput){
  DynamicJsonDocument jsonOutput(525);
  DeserializationError error = deserializeJson(jsonOutput, jsonInput);
  return jsonOutput;
}

class ReadData{
  DateTime readDate;
  float readValue;
  
  public:
  ReadData(DateTime _readDate, float _readValue){
    readDate = _readDate;
    readValue = _readValue;
  }
};


//Classe de Nível de Produção
class Level{
  int levelId;
  boolean levelStatus;
  boolean levelLightStatus;

  
  String levelCarbonPin;
  int levelTemperaturePin;
  int levelLightPins[2];
  String levelMoisturePins[3];

  
  Adafruit_NeoPixel levelLight[2];

  //Leituras em Tempo Real do Nível
  StackArray<ReadData> levelTemperature;
  StackArray<ReadData> levelHumidity;
  StackArray<ReadData> levelCarbon;
 
  
  public:
  Level(){
    levelStatus = false;
    levelLightStatus = false;
    //levelPresenceLight = false; 
    levelTemperature.setPrinter (Serial);
    levelHumidity.setPrinter (Serial);
    levelCarbon.setPrinter (Serial);
  }

  void cleanReadedData(){
    while (!levelTemperature.isEmpty()){
      levelTemperature.pop();
    }
    
    while (!levelHumidity.isEmpty()){
      levelHumidity.pop();
    }
       
    while (!levelCarbon.isEmpty()){
      levelCarbon.pop();
    }    
  }

  void serviceInit(String levelConfig){
    //Inicializando o modulo de Iluminação do Nível com base nas configurações
    levelLight[0] = Adafruit_NeoPixel(12, levelLightPins[0], NEO_GRB + NEO_KHZ800);
    levelLight[0].begin(); 
    levelLight[1] = Adafruit_NeoPixel(4, levelLightPins[1], NEO_GRB + NEO_KHZ800);
    levelLight[1].begin(); 


    
  }
  
  void serviceUpdate(DateTime upTime){
    //Se o Nível se encontra ativo executar operações
    if(levelStatus){
       //Leitura dos Dados Ambientais e Armazenamento em Variavel
       ReadData readedTemperature(upTime,0);
       levelTemperature.push(readedTemperature);
       
       ReadData readedHumidity(upTime,0);
       levelHumidity.push(readedHumidity);
       
       ReadData readedCarbon(upTime,0);
       levelCarbon.push(readedCarbon);

      //Se chegou a 10 leituras 
      if(levelCarbon.count() == 10){
        //saveLevelData();
        cleanReadedData();
      }
    }
  }

  void serviceDaemon(DateTime uptime){
    //Se o Nível se encontra ativo executar operações
    if(levelStatus){     
      //Verificação dos Horarios de Iluminação
      if(uptime.hour() >= 0 && uptime.hour() < 23){
        if(!levelLightStatus){
          levelLightStatus = true;
          Serial.println("Ligando apenas uma vez");
        }
      }else if(levelLightStatus){
        levelLightStatus = false;
        Serial.println("Desligando apenas uma vez");        
      }
    }
  }

  void levelPresenceOn(){
    //Ligando apenas se a Luz do Nível não estiver sendo utilizada
    if(levelStatus && !levelLightStatus){
      Serial.print("Ligando Luz do Nível em Branco Temporariamente");
    }else{
      Serial.print("Não posso ligar");
    }
  }

  void levelPresenceOff(){
    //Desligando apenas se a Luz do Nível não estiver sendo utilizada
    if(levelStatus && !levelLightStatus){
      Serial.print("Desligando Luz Branca");
    }else{
      Serial.print("Não posso desligar");
    }
  }

  //SETS//
  void set_LevelId(int _levelId){
    levelId = _levelId;
  }

  void set_LevelLightPins(int _levelLightPinLarge,int _levelLightPinSmall){
    levelLightPins[0] = _levelLightPinLarge;
    levelLightPins[1] = _levelLightPinSmall;
  }


  //GETS//
  int get_LevelId(){
    return levelId;
  }

  int* get_LevelLightPins(){
    return levelLightPins;
  }
  
};




//Classe da CPU (Torre)
class Cpu{
  //Dados da CPU (Torre)
  String cpuId;
  boolean cpuBoot;
  String cpuMode;
  
  //Dados referentes a Data/Hora e Perifericos
  RTC_DS3231 cpuTime;
  DateTime cpuBootTime;

  //Display
  Adafruit_SSD1306 cpuDisplay;


  //Dados de Execução em tempo Real
  long cpuUpdate;  
  int cpuPresencePin;
  int cpuPresenceInterval;
  boolean cpuPresenceLight;
  DateTime cpuPresenceTimeout;
  
  public:
    Level cpuLevels[3];

  //Construtor
  public:
  Cpu(){
    cpuDisplay = Adafruit_SSD1306(128, 64, &Wire, -1);
    cpuBoot = false; 
    cpuPresenceLight = false;

    //CPU Unique ID
    for (size_t i = 0; i < UniqueIDsize; i++){
      if (UniqueID[i] < 0x10)
        cpuId = cpuId + "0";
      cpuId = cpuId + UniqueID[i] + ((i == UniqueIDsize-1) ? "" : " ");
    }

    //CPU Default Configuration if not overwrite with a external config
    setCpuConfig(def_cpuConfig); 
  }

  /* CUSTOM CONSTRUCT */
  void setCpuConfig(String cpuConfig){
     DynamicJsonDocument configParsed(525);
     configParsed = jsonRead(cpuConfig);
     cpuPresencePin = configParsed["cpuPresencePin"];
     cpuPresenceInterval = configParsed["cpuPresenceInterval"];
     
     for(int i=0;i<3;i++){      
      cpuLevels[i].set_LevelId(configParsed["cpuLevels"][i]["levelId"]);
      //cpuLevels[i].set_LevelCarbonPin(configParsed["cpuLevels"][i]["levelCarbonPin"]);
      //cpuLevels[i].set_LevelTemperaturePin(configParsed["cpuLevels"][0]["levelTemperaturePin"]);
      cpuLevels[i].set_LevelLightPins(configParsed["cpuLevels"][i]["levelLightPins"][0],configParsed["cpuLevels"][i]["levelLightPins"][1]);
      //cpuLevels[i].set_LevelMoisturePins(configParsed["cpuLevels"][0]["levelMoisturePins"]);        


     }
  }

  /* GETS */
  String get_cpuId(){
    return cpuId;  
  }

  boolean get_cpuStatus(){
    return cpuBoot;  
  }

  String get_cpuMode(){
    return cpuMode;  
  }

  long get_cpuTime(){
    return cpuTime.now().unixtime();
  }
  
  long get_cpuUpdate(){
    return cpuUpdate;
  }
  
  /* SETS */
  void set_cpuUpdate(){
    cpuUpdate = cpuTime.now().unixtime();
  }

  /* FUNÇÕES */
  String serviceInit(int _cpuMode){ 
    
    //Validação do Status de Boot dentro da Thread
    boolean bootStatus = true;   
    
    //Definindo Modo de execução da CPU
    cpuMode = (_cpuMode == 1) ? "legacy" : "connected";

    //Definções de Pinagens
    pinMode(10,INPUT);

    //Error Code 
    String errorCode = "0x00";
    
    //Validações de Perifericos para Inicialização
    if(cpuDisplay.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      cpuDisplay.display();     
      if (cpuTime.begin()){
        //Verificar se existe a necessidade de atualização da hora
        if (cpuTime.lostPower()){
          if(cpuMode == "legacy"){
            Serial.println("É necessária uma atualização de data e hora é recomendado recompilar o codigo, ou inicializar a CPU no modo connected.");  
            cpuTime.adjust(DateTime(F(__DATE__), F(__TIME__)));    
          }else{
            //Request Data from Master
            Serial.println("{\"cpuUniqueId\": \"+cpuId+\",\"command\":\"requestDataUpdate\"}");
            
            //Update CPU Time based on Master Date/Time
            //Ler resposta da porta Serial
              boolean dataUpdated = false;
              //5 Segundos de Timeout aguardando a resposta, caso não ocorra 
              long timeOut = millis() + 3000;
              while(!dataUpdated & timeOut < millis()){
                String waitingResponse = serialRead();
                //Se a resposta for diferente de nulo
                if(waitingResponse != NULL){
                  //Verificando se a entrada de Dados Json é Valida para Data e referente a solicitação do Modulo
                  if(true){
                    //Ajustando hora com base no timestamp recebido (Pode haver pequenos atrasos);
                    //cpuTime.adjust();
                    dataUpdated = true;
                  }
                }
              }
              bootStatus = dataUpdated;
              errorCode = "0x03";
           }

            //Falta a atualização
          }
        }else{
          errorCode = "0x02";
          bootStatus = false;
        }
      }else{
        errorCode = "0x01";
        bootStatus = false;
      }
    


    if(_cpuMode == 1){
      if(bootStatus){
      //Mensagem para o modo Connected
        cpuBoot = true;
        return("Inicialização da CPU Executada com sucesso");
      }else{
        return("Erro durante a inicialização da CPU. COD: "+errorCode);
      }
    }else{
      if(bootStatus){
      //Handshake para o modo Connected
        cpuBoot = true;
        return("{\"cpuUniqueId\": 1,\"command\":\"setInitStatus\" \"message\":\"initSuccess\"}");
      }else{
        return("{\"cpuUniqueId\": 1,\"command\":\"setInitStatus\",\"message\":[\"initFailed\",\""+errorCode+"\"]}");
      }
    }
  }

  //Update based on 10s delay
  void serviceUpdate(){
    if(true){

      Serial.print(cpuTime.now().hour(), DEC);
      Serial.print(':');
      Serial.print(cpuTime.now().minute(), DEC);
      Serial.print(':');
      Serial.print(cpuTime.now().second(), DEC);
      Serial.println();

      cpuLevels[0].serviceUpdate(cpuTime.now());
      cpuLevels[1].serviceUpdate(cpuTime.now());
      cpuLevels[2].serviceUpdate(cpuTime.now());
      
      //Enviando resposta em tempo real para o Mestre sobre as informações dos níveis de produção
      if(cpuMode == "connected"){
        //Handshake para o modo Connected
        Serial.println("????"); 
      }
    }
  }

  //Realtime updated nedded Daemon
  void serviceDaemon(){
    
    //Read Serial Input if CPU connected to Master
    if(cpuMode == "connected"){
      
      //Ler resposta da porta Serial
      String response = serialRead();
      
      //Se a resposta for diferente de nulo
      if(response != NULL){
        //Tratamentos de entrada de dados
      }
    }

    //Atualizações em Tempo Real para o nível de produção
    cpuLevels[0].serviceDaemon(cpuTime.now());
    cpuLevels[1].serviceDaemon(cpuTime.now());
    cpuLevels[2].serviceDaemon(cpuTime.now());  

    //Logica do Sensor de presença
    if(cpuPresenceInterval != -1){
      if(digitalRead(10)){
          if(!cpuPresenceLight){
            cpuPresenceLight = true;
            cpuLevels[0].levelPresenceOn();
            cpuLevels[1].levelPresenceOn();
            cpuLevels[2].levelPresenceOn();
            cpuPresenceTimeout = cpuTime.now();
          }
      }else if(cpuPresenceLight == true && ( (cpuTime.now().unixtime()) >  (cpuPresenceTimeout.unixtime() + cpuPresenceInterval ))){
        Serial.println(cpuPresenceTimeout.unixtime()+20);
        Serial.println("<>");
        Serial.println(cpuTime.now().unixtime());
        cpuPresenceLight = false;
        cpuLevels[0].levelPresenceOff();
        cpuLevels[1].levelPresenceOff();
        cpuLevels[2].levelPresenceOff();
      }
    }
  }


};


//Criando Objeto da CPU;    
Cpu cpuObject;

void(* resetFunc) (void) = 0;

void setup(){
    
  //Inicializando Porta Serial
  Serial.begin(9600);

  cpuObject.setCpuConfig(def_cpuConfig);
  //cpuObject.setCpuConfig("");
  //Boot Time for Conection Timeout
  long bootTime = millis();

  //Conection Status Attempt
  boolean conectionStatus = false;
  
  //Timeout (10s) para inicialização da conexão serial com o Mestre
   delay(10000);

  //Convite de Conexão
  Serial.println("{\"cpuUniqueId\": \""+cpuObject.get_cpuId()+"\",\"message\":\"requestConnection\"}");
  //Timeout (10s) para resposta da CPU no modo Connected com base nas especificações do Mestre
  while(millis() < 20000){
    String response = serialRead();
    if(response != NULL){
      conectionStatus = true;
      break;
    }
  }
  

  if(!conectionStatus){
    //Inicialização no Modo Connected
    Serial.println("Inicializando PocketFarm Client no modo Legacy (Offline)");
  }

  //Inicialização com base no Modo
  Serial.println(cpuObject.serviceInit(!conectionStatus ? 1 : 2));
}




void loop(){
  
  //Update caso a CPU esteja em Funcionamento
  if(cpuObject.get_cpuStatus()){

    //Atualizações basicas em Tempo Real
    cpuObject.serviceDaemon();

    //Atualizações da CPU intervalares com espaço de 10s
    if((cpuObject.get_cpuTime() - cpuObject.get_cpuUpdate()) > 10){
      cpuObject.serviceUpdate();
      //cpuObject.set_cpuUpdate();
    }
  }else{
    Serial.println(cpuObject.cpuLevels[2].get_LevelLightPins()[0]);
    Serial.println("Executando nova tentativa de Inicializando em: 3");
    delay(1000);
    Serial.println("Executando nova tentativa de Inicializando em: 2");
        Serial.println(cpuObject.cpuLevels[1].get_LevelId());
    delay(1000);
    Serial.println("Executando nova tentativa de Inicializando em: 1");
        Serial.println(cpuObject.cpuLevels[2].get_LevelId());
    Serial.println("-------------------------------------------------");
    delay(1000);
    resetFunc();

  }
}
