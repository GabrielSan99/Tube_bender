#include <DNSServer.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include <SPIFFS.h>

DNSServer dnsServer;
AsyncWebServer server(80);

// Network credentials
const char* ssid     = "Tube Bender AP";
const char* password = "ifsp1234";

// Talvez eu tenha que mudar para micros() o delay para não travar a verificação do código
const int ena = 25;         //enable the motor
const int dir = 26;         //determine the direction
const int pul = 27;         //execute one step
const int interval = 350;   //interval between the pulse state changes (micro seconds)
boolean pulse = LOW;        //pulse state

int degree;

bool degree_received = false;
bool stop_received = false;
bool right_received = false;
bool left_received = false;


// CCW  --> Direction to bend the tube (High)
// CW   --> Direction to return initial position (Low)

// Motor Resolution   --> 1.8 */step == 200 steps/turn

//One pulse has 700 microseconds of duration
//One turn has 140 miliseconds of duration

void turn_left(float turn){
  
  int aux;
  int pulses = 200 * turn;
  Serial.println(pulses);
  
  digitalWrite(dir, LOW); 
  digitalWrite(pul, HIGH);
  
  for (aux = 0; aux <= pulses; aux ++){
      pulse = !pulse;
      digitalWrite(pul, pulse);
      delayMicroseconds(interval);
      
      pulse = !pulse;
      digitalWrite(pul, pulse);
      delayMicroseconds(interval);    
  }  
}

void turn_right(float turn){
  
  int aux;
  int pulses = 200 * turn;
  Serial.println(pulses);
  
  digitalWrite(dir, HIGH); 
  digitalWrite(pul, HIGH);
  
  for (aux = 0; aux <= pulses; aux ++){
      pulse = !pulse;
      digitalWrite(pul, pulse);
      delayMicroseconds(interval);
      
      pulse = !pulse;
      digitalWrite(pul, pulse);
      delayMicroseconds(interval);    
  }
}

void tube_bender(int toDegree){
  
//  To reducer complete one turn is necessary 30 nema turns (1-30)
//  This allows find the relationship between motor and reducer ---> 30/360 = 0.083 nematurns/degree

  float turns;
  
  turns = toDegree*0.083;
  turn_right(turns);
  delay(2000);
  turn_left(turns);
  
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
    <title>Tube Bender</title>
    <meta name="viewport" id="viewport" content="width=device-width, height=device-height, initial-scale=1.0, maximum-scale=1.0, user-scalable=0"/>

    <script>
      // Global boolean variable that holds the current orientation
      var pageInPortraitMode;
      
      // Listen for window resizes to detect orientation changes
      window.addEventListener("resize", windowSizeChanged);
      
      // Set the global orientation variable as soon as the page loads
      addEventListener("load", function() {
        pageInPortraitMode = window.innerHeight > window.innerWidth;
        document.getElementById("viewport").setAttribute("content", "width=" + window.innerWidth + ", height=" + window.innerHeight + ", initial-scale=1.0, maximum-scale=1.0, user-scalable=0");
      })
      
      // Adjust viewport values only if orientation has changed (not on every window resize)
      function windowSizeChanged() {
        if (((pageInPortraitMode === true) && (window.innerHeight < window.innerWidth)) || ((pageInPortraitMode === false) && (window.innerHeight > window.innerWidth))) {
          pageInPortraitMode = window.innerHeight > window.innerWidth;
          document.getElementById("viewport").setAttribute("content", "width=" + window.innerWidth + ", height=" + window.innerHeight + ", initial-scale=1.0, maximum-scale=1.0, user-scalable=0");
      }
    }
    </script>
    
    </head><body style=background-color:#33475b;width:100%>
    <h1 style=display:flex;justify-content:center;text-align:center;height:15%;width:100%;color:white;font-size:40px>Tube Bender<br>Controller</h1>
    <img style=max-width:100%;height:auto;position:absolute;top:7%; src="data:image/png;base64,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">

    <form action="/get" style=text-align:center;position:absolute;top:50%;width:100%>
      <label for="degree" style=text-align:center;font-size:20px;color:white;>Enter the degree of curvature (1-180º):</label><br><br>
      <input style=width:80%;height:30px;text-align:center type="number" id="degree" name="degree" min="1" max="180" value=ssid><br><br>
      <input style=width:60%;height:40px;background-color:gray;font-weight:bold type="submit" value="Start">
    </form>
    <form action="/get" style=position:absolute;top:75%;display:flex;justify-content:center;width:100%>
        <div style=position:absolute;left:5%;background-color:bisque>
            <input style=background-color:lightgrey;width:90px;height:80px;font-weight:bold type="submit" id="left" name="left" value="LEFT"></input>
        </div>
        <div style=position:absolute;right:5%;background-color:blue>
            <input style=background-color:lightgrey;width:90px;height:80px;font-weight:bold type="submit" id="right" name="right" value="RIGHT"></input>
        </div>
        <br>         
        <div style=position:absolute;top:40px;width:30%;background-color:red>
            <input style=background-color:red;width:100%;height:80px;font-weight:bold type="submit" id="stop" name="stop" value="STOP"></input>
        </div>
    </form>
    </body></html>
      )rawliteral";

class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request){
    //request->addInterestingHeader("ANY");
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html); 
  }
};

void setupServer(){
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send_P(200, "text/html", index_html); 
      Serial.println("Client Connected");
  });
  

  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
      String inputDegree;
      String stopButton;
      String leftButton;
      String rightButton;
      

      if (request->hasParam("degree")) {
        inputDegree = request->getParam("degree")->value();
        degree = inputDegree.toInt();
        degree_received = true;
        //Serial.println((inputMessage.toInt()) + 100);
        request->send(200, "text/html", index_html);
      }

      if (request->hasParam("stop")) {
        stopButton = request->getParam("stop")->value();
        Serial.println("Stop");
        stop_received = true;
        request->send(200, "text/html", index_html);
      }

      if (request->hasParam("right")) {
        rightButton = request->getParam("right")->value();
        Serial.println("Right");
        right_received = true;
        request->send(200, "text/html", index_html);
      }

      if (request->hasParam("left")) {
        leftButton = request->getParam("left")->value();
        Serial.println("Left");
        left_received = true;
        request->send(200, "text/html", index_html);
      }

      
  });
}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}



void setup()
{
  Serial.begin(115200);
  pinMode(ena, OUTPUT);
  pinMode(dir, OUTPUT);
  pinMode(pul, OUTPUT);
  
  digitalWrite(ena, LOW); 


  Serial.println("Setting up AP Mode");
  WiFi.mode(WIFI_AP); 
  WiFi.softAP(ssid, password);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("Setting up Async WebServer");
  setupServer();
  Serial.println("Starting DNS Server");
  dnsServer.start(53, "*", WiFi.softAPIP());
  server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);//only when requested from AP

  server.onNotFound(notFound);
  server.begin();
  Serial.println("All Done!");

}


void loop()
{
  dnsServer.processNextRequest();
  if(stop_received){
    Serial.println("Stoped");
    stop_received = false;
  }
  if(degree_received){
      Serial.println("Starting " + String(degree) + " degree bend!");
      tube_bender(degree);
      degree_received = false;
    }
  if(right_received){
    turn_right(0.415);
    right_received = false;
  }
  if(left_received){
    turn_left(0.415);
    left_received = false;
  }
}
