#include <WiFi.h> //Connect to WiFi Network
#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h> //Used in support of TFT Display
#include <string.h>  //used for some string handling and processing.
#include <math.h> //used for analog clock face

#define IDLE 0  //Initial State
#define DOWN 1  //State when the button is pressed down
#define UP 2  //State when the button comes back up
#define DISPLAY 3 //Sttate to update our display


TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h

//Some constants and some resources:
const uint16_t RESPONSE_TIMEOUT = 6000; //ms to wait for response from host
const uint16_t SERVER_PULL_TIME = 60000; //periodicity of pulling from server.
const uint16_t DISPLAY_UPDATE_TIME = 1000; //periodicity of updating the screen
const uint16_t IN_BUFFER_SIZE = 1000; //size of buffer to hold HTTP request
const uint16_t OUT_BUFFER_SIZE = 1000; //size of buffer to hold HTTP response
char request_buffer[IN_BUFFER_SIZE]; //char array buffer to hold HTTP request
char response_buffer[OUT_BUFFER_SIZE]; //char array buffer to hold HTTP response

char network[] = "MIT";  //SSID for MIT open network
char password[] = ""; //No Password required for open network

const int input_pin = 16; //pin connected to button
uint8_t state = IDLE;  //initialize to IDLE
uint8_t watch_state = 0; //variable for state of the watch (0 for digital, 1 for digital)

unsigned int server_timer;  //time from last server pull
unsigned int display_timer; //timer for updating the display when not pressed

uint8_t hour; //stored times from last server pull
uint8_t minute;
uint8_t second;

uint8_t display_hour; //times to be displayed now
uint8_t display_minute;
uint8_t display_second;


void setup(){
  tft.init();  //init screen
  tft.setRotation(2); //adjust rotation
  tft.setTextSize(2); //default font size
  tft.fillScreen(TFT_BLACK); //fill background
  tft.setTextColor(TFT_GREEN, TFT_BLACK); //set color of font to green foreground, black background
  
  Serial.begin(115200); //begin serial comms
  delay(100); //wait a bit (100 ms)
  WiFi.begin(network,password); //attempt to connect to wifi
  uint8_t count = 0; //count used for Wifi check times
  Serial.print("Attempting to connect to ");
  Serial.println(network);
  
  while (WiFi.status() != WL_CONNECTED && count<6) {
    delay(500);
    Serial.print(".");
    count++;
  }
  delay(2000);
  if (WiFi.isConnected()) { //if we connected then print our IP, Mac, and SSID we're on
    Serial.println("CONNECTED!");
    Serial.printf("%d:%d:%d:%d (%s) (%s)\n",WiFi.localIP()[3],WiFi.localIP()[2],
                                            WiFi.localIP()[1],WiFi.localIP()[0], 
                                          WiFi.macAddress().c_str() ,WiFi.SSID().c_str());
    delay(500);
  } else { //if we failed to connect just Try again.
    Serial.println("Failed to Connect :/  Going to restart");
    Serial.println(WiFi.status());
    ESP.restart(); // restart the ESP (proper way)
  }
  
  pinMode(input_pin, INPUT_PULLUP); //set input pin as an input
  state = IDLE; //start system in IDLE state
  update_from_server();
  display_watch();

  
  
}

void loop(){
  number_fsm(digitalRead(input_pin)); //Call our FSM every time through the loop.
  delay(10);
}



/*------------------
 * number_fsm Function:
 * Use this to implement your finite state machine. It takes in an input (the reading from a switch), and can use
 * state as well as other variables to be your state machine.
 */

void number_fsm(uint8_t input){
  //your logic here!
  //use your inputs and the system's state to update state and perform actions
  //This function should be non-blocking, meaning there should be minimal while loops and other things in it!
  //Feel free to call do_http_GET from this function
  //state variable globally defined at top
  bool unpressed = input;

  if ((millis() - server_timer) > SERVER_PULL_TIME){
    update_from_server();
  }
  if ((millis() - display_timer) > DISPLAY_UPDATE_TIME){
    display_watch();
  }
  
  switch(state){
    case IDLE:
      if (unpressed == 0){ //button is down, new state
        state = DOWN;
      }
      break;
    case DOWN:
      if (unpressed == 1){ //button is back up, end of a press-release sequence
        state = UP;
      }
      break;
    case UP:
      watch_state = (watch_state + 1) % 2; //Change watch state from analog to digital
      state = DISPLAY; //change display
      break;
      
    case DISPLAY:
      display_watch(); //display new watch face
      state = IDLE; //return to IDLE state
      break;
  }
  
}

/*----------------------------------
 * display_watch Function:
 * 
 * Display the correct watch face based on the state of the watch
 */
void display_watch(){
  if (watch_state == 0){ //digital
    display_digital();
  }else{                 //analog
    display_analog();
  }
  display_timer = millis();
}


/*----------------------------------
 * update_display_times Function:
 * 
 * Update times to display based on how long has passed since the last server pull
 */
void update_display_times(){
  int seconds_passed = (millis() - server_timer) / 1000; //collect time passed since last update
  int minutes_passed = seconds_passed / 60;
  int hours_passed = minutes_passed / 60;

  display_second = second + (seconds_passed % 60); //add time passed to each field
  display_minute = minute + (minutes_passed % 60);
  display_hour = hour + (hours_passed % 24);

  if (display_second >= 60){  //adjust fields modulo their max values and carry over the remainder
    int extra_min = display_second / 60;
    display_second = display_second % 60;
    display_minute = display_minute + extra_min;
  }

  if (display_minute >= 60){
    int extra_hours = display_minute / 60;
    display_minute = display_minute % 60;
    display_hour = hour + extra_hours;
  }

  if (display_hour >= 24){
    display_hour = display_hour % 24;
  }
  
}



/*----------------------------------
 * display_digital Function:
 * 
 * displays the digital state of the watch
 */
void display_digital(){
  tft.setRotation(3); //adjust rotation
  update_display_times(); //update times to be displayed
  tft.fillScreen(TFT_BLACK); //black out TFT Screen
  
  char display_buffer[20]; //buffers to hold string display output
  char hour_buffer[10];
  char minute_buffer[10];
  char second_buffer[10];

  if (display_hour < 10){ //pads with an extra zero if needed
    sprintf(hour_buffer, "0%d", display_hour);
  }else{
    sprintf(hour_buffer, "%d", display_hour);
  }

  if (display_minute < 10){
    sprintf(minute_buffer, "0%d", display_minute);
  }else{
    sprintf(minute_buffer, "%d", display_minute);
  }

  if (display_second < 10){
    sprintf(second_buffer, "0%d", display_second);
  }else{
    sprintf(second_buffer, "%d", display_second);
  }

  strcat(display_buffer, hour_buffer); //concatentates output
  strcat(display_buffer, ":");
  strcat(display_buffer, minute_buffer);
  strcat(display_buffer, ":");
  strcat(display_buffer, second_buffer);
  
  tft.drawString(display_buffer, 35, 50, 1); //viewable on Screen
}


/*----------------------------------
 * display_digital Function:
 * 
 * displays the analog state of the watch
 */
void display_analog(){
  tft.setRotation(2); //adjust rotation
  tft.fillScreen(TFT_BLACK);
  update_display_times(); //update times to be displayed
  
  int hour_len = 20; //length of clock hands
  int minute_len = 35;
  int second_len = 45;

  float hour_frac = ((float)(display_hour % 12) / (12.0)); //fraction of the way around the clock for each hand
  float minute_frac = ((float)display_minute / (60.0));
  float second_frac = ((float)display_second / (60.0));

  float hour_rads = (hour_frac * 2 * M_PI) + ((2 * M_PI / 12.0)*(minute_frac)); //angle of each hand from original position
  float minute_rads = minute_frac * 2 * M_PI;
  float second_rads = second_frac * 2 * M_PI;

  float hour_cos = cos(hour_rads); //sin and cosine for each hand
  float hour_sin = sin(hour_rads);
  float minute_cos = cos(minute_rads);
  float minute_sin = sin(minute_rads);
  float second_cos = cos(second_rads);
  float second_sin = sin(second_rads);

  float hour_y = hour_len * hour_sin; //new positions for each hand
  float hour_x = hour_len * hour_cos;

  float minute_y = minute_len * minute_sin;
  float minute_x = minute_len * minute_cos;

  float second_y = second_len * second_sin;
  float second_x = second_len * second_cos;
  

  tft.drawCircle(64,80,50,TFT_GREEN); //display clock face
  tft.drawLine(64, 80, hour_x + 64.0, hour_y + 80.0, TFT_GREEN);
  tft.drawLine(64, 80, minute_x + 64.0, minute_y + 80.0, TFT_GREEN);
  tft.drawLine(64, 80, second_x + 64.0, second_y + 80.0, TFT_GREEN);
  
}


/*----------------------------------
 * extract_server_response Function:
 * 
 * After server respone has been received, extract the relevant time data and store it
 */
void extract_server_response(){
  char space[] = " "; //Split the response string by a blank space
  char colon[] = ":"; //Split the time string by a colon
  char dot[] = "."; //Split the seconds string by a colon

  char *date_str = strtok(response_buffer, space); //date section
  char *new_time = strtok(NULL, space); //time section

  char time_str[50];
  strcpy(time_str, new_time); //new string to prevent overwriting old one

  char *hour_str[] = {strtok(time_str, colon)}; //extract hour, min, and sec from the time string
  char *minute_str[] = {strtok(NULL, colon)};
  char *second_str[] = {strtok(NULL, colon)};

  char second_copy[10];
  strcpy(second_copy, *second_str); //new string to prevent overwriting old one

  char *sec_str[] = {strtok(second_copy, dot)}; //extract the integer component of the second

  int hour_int = atoi(*hour_str); //convert all times to int
  int minute_int = atoi(*minute_str);
  int second_int = atoi(*sec_str);

  hour = hour_int;
  minute = minute_int;
  second = second_int;
}

/*----------------------------------
 * update_from_server Function:
 * 
 * Pulls information from the tine server and resets the server timer.
 * Updates our previous idea of what time it was
 */
void update_from_server(){
  sprintf(request_buffer,"GET http://iesc-s3.mit.edu/esp32test/currenttime\r\n"); //Time API URL
  strcat(request_buffer,"Host: iesc-s3.mit.edu\r\n"); //add more to the end
  strcat(request_buffer,"\r\n"); //add blank line!
  //submit to function that performs GET.  It will return output using response_buffer char array
  do_http_GET("iesc-s3.mit.edu", request_buffer, response_buffer, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT,true);

  extract_server_response();

  Serial.println(hour);
  Serial.println(minute);
  Serial.println(second);

  server_timer = millis(); //reset server timer
  
}



/*----------------------------------
 * char_append Function:
 * Arguments:
 *    char* buff: pointer to character array which we will append a
 *    char c: 
 *    uint16_t buff_size: size of buffer buff
 *    
 * Return value: 
 *    boolean: True if character appended, False if not appended (indicating buffer full)
 */
uint8_t char_append(char* buff, char c, uint16_t buff_size) {
        int len = strlen(buff);
        if (len>buff_size) return false;
        buff[len] = c;
        buff[len+1] = '\0';
        return true;
}

/*----------------------------------
 * do_http_GET Function:
 * Arguments:
 *    char* host: null-terminated char-array containing host to connect to
 *    char* request: null-terminated char-arry containing properly formatted HTTP GET request
 *    char* response: char-array used as output for function to contain response
 *    uint16_t response_size: size of response buffer (in bytes)
 *    uint16_t response_timeout: duration we'll wait (in ms) for a response from server
 *    uint8_t serial: used for printing debug information to terminal (true prints, false doesn't)
 * Return value:
 *    void (none)
 */
void do_http_GET(char* host, char* request, char* response, uint16_t response_size, uint16_t response_timeout, uint8_t serial){
  WiFiClient client; //instantiate a client object
  if (client.connect(host, 80)) { //try to connect to host on port 80
    if (serial) Serial.print(request);//Prints the request we are sending to Serial Monitor
    client.print(request);//Sends the request
    memset(response, 0, response_size); //Null out (0 is the value of the null terminator '\0') entire buffer
    client.readBytesUntil('\n',response,response_size); //Read the one line response
    if (serial) Serial.println(response);//Print out response to Serial Monitor
    client.stop();
    if (serial) Serial.println("-----------");//End of Serial Monitor printing
  }else{
    if (serial) Serial.println("connection failed :/");
    if (serial) Serial.println("wait 0.5 sec...");
    client.stop();
  }
}        
