/* 
// CANDY DISPENSER USING PARTICLE ARGON AND BLYNK
 * Author: Sama Shah
 * Date: 12/08/2023
 * Description: Saunf Dispenser: a vending machine-style dispenser to dispense saunf, or fennel seeds, which is popular in india as a post meal mouth freshener popular for its rich digestive elements and antioxidants
 * For comprehensive documentation and examples, please visit:
 * https://docs.particle.io/firmware/best-practices/firmware-template/
 */

//SECTION 1: INITIALIZING LIBRARIES AND GLOBAL VARIABLES

// Include Particle Device OS APIs
#include "Particle.h"

// //OLED I2C wire library:
// #include <Wire.h>

//define blynk
#define BLYNK_TEMPLATE_ID //enter blynk template ID
#define BLYNK_TEMPLATE_NAME //enter project name
#define BLYNK_AUTH_TOKEN //enter blynk auth token

#include <blynk.h>
// #define BLYNK_IP IPAddress(64, 225, 16, 22)
#define BLYNK_PRINT Serial

// Let Device OS manage the connection to the Particle Cloud
SYSTEM_MODE(AUTOMATIC);

// Run the application and system concurrently in separate threads
SYSTEM_THREAD(ENABLED);

// Show system, cloud connectivity, and application logs over USB
// View logs with CLI using 'particle serial monitor --follow'
SerialLogHandler logHand;

//dht20
#include "DHT20.h"
DHT20 dht;

//oled library
#include "SparkFunMicroOLED.h"  
#include "math.h"

// OLED display object
// MicroOLED oled; 
MicroOLED oled(MODE_I2C, D7, 1);

// Variables to store sensor data
float temp;

//photoresistor sensor
const int PIN_PHOTORESISTOR = A5;

//servo
const int SERVO_PIN = D2;
// #1 create the servo object
Servo servo;  // no need to #include library

//rgb physical display
const int PIN_RGB_RED = D4;
const int PIN_RGB_GREEN = D5;
const int PIN_RGB_BLUE = D6;

bool rgbGreen = FALSE; // this is important to ensure that when someone has not payed, the dispenser aka the servo won't work

//initial state payment/revenue tracker
float payment_revenue;

//photoresistor thresholds specific to my location
const int THRESHOLD_DARK = 800;    

int payVal; //this is the variable where we will store the state of the blynk pay button (on/off;1/0)

//SECTION 2: change rgb color function
void changeRgbColor(int r, int g, int b) {
  analogWrite(PIN_RGB_RED, r);
  analogWrite(PIN_RGB_GREEN, g);
  analogWrite(PIN_RGB_BLUE, b);
}

//my Handler handles the integration response of initial state. 
void myHandler(const char *event, const char *data) {} //for initial state integration

//SECTION 3: Setup function sets up pinModes of the rgb pins, begins the dht sensor, oled, blynk app interface, servo, and the initial state subscription.
void setup() {
  Serial.begin(9600);
  pinMode(PIN_RGB_RED, OUTPUT);
  pinMode(PIN_RGB_GREEN, OUTPUT);
  pinMode(PIN_RGB_BLUE, OUTPUT);
  pinMode(PIN_PHOTORESISTOR, INPUT);

  //initialize DHT software object
  dht.begin();

  // Initialize & prepare OLED
  oled.begin();        // Initialize the OLED
  oled.clear(ALL);     // Clear the display's internal memory
  oled.display();      // Display what's in the buffer (splashscreen)
  delay(1000);         // Delay 1000 ms
  oled.clear(PAGE);

  //initialize blynk
  Blynk.begin(BLYNK_AUTH_TOKEN);
  Particle.subscribe("hook-response/saunf_payment", myHandler, MY_DEVICES);

  //ok to attach servo here if servo is not jittering. otherwise use it in loop whenever in usage
  servo.attach(SERVO_PIN);
}

//SECTION 4: this is the main functionality of the blynk app which is how we are able to control the argon with user interaction on the mobile interface. 

  // blynk virtual write
  // FROM APP to ARGON
  BLYNK_WRITE(V0)  // functionality of "pay" button -- THIS IS A FUNCTION
  {
      
      payVal = param.asInt(); //0 is false/not-clicked, 1 is true/clicked

      if (payVal == 1) {
          //if pour button is on, virtual write rgb green to TRUE and change rgb color to green and publish payment to initial state
          payment_revenue += 0.25; //revenue value tracker since each payment is $0.25
          Particle.publish("saunf_payment", "$"+ String(payment_revenue,2), PRIVATE); //publish to display revenue on initial state (communicating to the cloud)
          changeRgbColor(0, 255, 0); //green
          rgbGreen = TRUE;

          //display messages to oled:
          // Print to OLED
          oled.clear(PAGE);
          oled.setFontType(0);   // Set font to type 0
          oled.setCursor(0, 0);  // Set cursor to top-left

          //optional: display actual temperature
          // oled.println("Temp");
          // oled.println(String(temp, 1) + " F");

          //message after user clicks pay on blynk app. this allows user to dispense food
          oled.println("Payed! Dispenser Activated...");
          //"enjoy the ___ weather!"
          if (temp >70) { //this message changes based on the temperature detected by the dht sensor
            oled.println("Enjoy the warm weather!");
          } else if (56 < temp < 70) {
            oled.println("Enjoy the cloudy weather!");
          } else {
            oled.println("Enjoy the chilly weather!");
          }

          oled.display();

      } else {
        changeRgbColor(255,0,0); //back to red light
        rgbGreen = FALSE; //tracking state of the green light with the on value of the pay button allows the red light is being tracked with the pay button's false value so that we can have more functionality with it like locking dispenser when user has not paid
      }


  }

  //SECTION 5: loop function is the core logic that loops on and on. This is where the actual servo dispensing actions take place.

void loop() {
  
  //run blynk functions (aka vitrtual write V0 func)
  Blynk.run();

  //for dht temp sensor
  int status = dht.read();

  temp = dht.getTemperatureF(); //used to display appropriate weather message on oled displayed in the blynk function

  // serial debugging and monitoring
  // Serial.println("Temp F: "+ String(temp, 1));


  //photoresistor readings
  int lightReading = analogRead(PIN_PHOTORESISTOR);
  // float voltage = float(lightReading) * 3.3 / 4095;

  //useful for determining thresholds specific to the light in my environment at a given time


  /*
    MY LIGHT THRESHOLDS (as of writing this code)
    0-1800 = dark
    1801--4095 = bright
  */

// SECTION 6: Conditionals 
  if (lightReading >= 0 && lightReading <= THRESHOLD_DARK && payVal == 1) { //light will only move servo (dispense) once pay button is clicked. this will also pour when sensor detects "dark" threshold, aka the users hand hovers over the photresistor
      Serial.println("Dark");
      //because rgbGreen is true for this to run, the light and payment already "went through" in the blynk virtual write function
      //while button pressed, change servo angle in order to dispense saunf 
      // servo.attach(SERVO_PIN); //attach here instead of setup to avoid jittering
      servo.write(100);  //100 deg angle
      // servo.detach(); //detach to avoid jittering


  } else if (lightReading >= 0 && lightReading >= THRESHOLD_DARK && payVal == 1) { //this allows dispenser to stay idle in the moment when the payment is complete but the user's hand has not hovered over the light sensor yet
      changeRgbColor(0, 255, 0); //still green
      rgbGreen = TRUE;
      // servo.attach(SERVO_PIN); //attach here instead of setup to avoid jittering
      servo.write(0);  //0 deg angle
      // servo.detach(); //detach to avoid jittering

  }  else if (lightReading >= 0 && lightReading <= THRESHOLD_DARK && payVal == 0  && rgbGreen == FALSE) { //this locks the dispenser when the payment is not complete, aka the pay button is off. rgbGreen bool here makes sure when the pay button is off and the light turns red, you cant dispense food
      changeRgbColor(255, 0, 0); //red
      // Print to OLED
          oled.clear(PAGE);
          oled.setFontType(0);   // Set font to type 0
          oled.setCursor(0, 0);  // Set cursor to top-left
          oled.println("Pay to dispense!");
          oled.display();

  } else { //this basically resets the rgbGreen state to communicate to the program that there is no payment and any other actions in this off state can work when needed
      changeRgbColor(255, 0, 0); //red
      // servo.attach(SERVO_PIN); //attach here instead of setup to avoid jittering
      //write servo angle
      servo.write(0);  //0 deg angle
      // servo.detach(); //detach to avoid jittering
      rgbGreen = FALSE;

  }

  Serial.println(lightReading);

}

