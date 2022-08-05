/* Blynk cloud connection with SIM800 GSM module
    Fill in the credentials obtained from your Blynk account
*/
#define BLYNK_TEMPLATE_ID " " //  Blynk template ID from your blynk.io cloud account
#define BLYNK_DEVICE_NAME "IoT Voting System" //  Device name can go here
#define BLYNK_AUTH_TOKEN " " // Blynk auth token from your blynk.io account
#define TINY_GSM_MODEM_SIM800 //  TinyGSM SIM800L module library
//  Include all necessary libraries
#include <EEPROM.h> //  EEPROM library to store data
#include <TinyGsmClient.h> //  TinyGSM module client
#include <BlynkSimpleTinyGSM.h> //  TinyGSM Blynk library
#include <Adafruit_Fingerprint.h> //  Adafruit generic fingerprint library - I used AS608 fingerprint module
#include <LiquidCrystal.h>  //  LCD module library & config
#include <SPI.h>  //  SPI library
#include <SD.h> //  SD card library, SD module pins on Mega2560 - 50 - MISO, 51 - MOSI, 52 - SCK, 53 - CS
#include <Keypad.h> //  Keypad module library
//  Libraries constants for initialization
const char apn[] = " ", user[] = "", pass[] = ""; //  Set your ISP apn value here, username and password are both optional
const char keys[4][4] = { //  Keypad 4x4 config
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
const byte rowPins[4] = {A8, A9, A10, A11}; //  Keypad row pins on board
const byte colPins[4] = {A12, A13, A14, A15}; //  Keypad column pins on board
//  Initialize all libraries
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, 4, 4 ); //  Initialize keypad module
TinyGsm modem(Serial1); //  SIM800L module placed on Serial1 of Mega2560
LiquidCrystal lcd(13, 12, 58, 59, 60, 40);  //  LCD module pins on board - RS,EN,D4,D5,D6,D7
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&Serial2);  //  Initiate fingerprint instance on Serial2
//  IDE constants
#define buzzer 41   //  Buzzer for communication
#define battPin A7  //  Analog pin to measure system's battery level
//  Global variables
bool localMode; //  Boolean to set device data logging mode, either locally to SD card or online to Blynk cloud
char passcode[4]; //  Universal array to store device admin passcode
char str1[4];   //  Universal array to store input from keypad
int batteryPercent; //  Battery percentage level
//  The following variables are made volatile to ensure high form of accuracy and precision
volatile int voted_users_count;  //  Number of users that have voted
volatile int usersid = EEPROM.read(0);  //  Number of registered users
volatile int a_count = EEPROM.read(1); //  Users that voted for Candidate A stored on EEPROM address 1
volatile int b_count = EEPROM.read(2); //  Users that voted for Candidate B stored on EEPROM address 2
volatile int c_count = EEPROM.read(3); //  Users that voted for Candidate C stored on EEPROM address 3
volatile int d_count = EEPROM.read(4); //  Users that voted for Candidate D stored on EEPROM address 4
volatile int voted_users[121];  //  Array to temporarily store users that have voted to prevent re-voting

void setup() {
  pinMode(53, OUTPUT);  //  SD card module CS pin
  pinMode(buzzer, OUTPUT);  //  buzzer digital output
  lcd.begin(20, 4); lcd.clear(); //  Initialize LCD and clear screen
  Serial.begin(9600); //  Initialize the serial monitor
  Serial.println("Device is starting");
  Serial.println("Awaiting operation mode selection");
  //  Prompt the user to either log locally on SD card or online to blynk cloud
  lcd.print("1 - Local Operation");
  lcd.setCursor(0, 1);  lcd.print("2 - Connect to Web");
  lcd.setCursor(2, 3);  lcd.print("Press '1' or '2'  ");
  const char text[][20] = {("IoT Advanced Voting "), ("System"), ("Connecting to web...")}; //  Scrolling text
  while (1) { //  Keypad prompt for input, infinite until key '1' or '2' is pressed
    char key = keypad.getKey();
    if (key == '1') {  //  Pressing key 1 indicates you choose to log data locally on SD card
      localMode = 1;  //  Used in loop to trigger SD card data logging actions
      break;
    } else if (key == '2') { //  Pressing key 2 indicates you choose to log data online to blynk cloud
      Serial1.begin(115200);  //  Initialize the baud rate of serial1 - SIM module
      lcd.clear();
      lcd.setCursor(0, 1);
      for (byte i = 0; i < 20; i++) { //  Displays scrolling text on LCD
        lcd.cursor();
        lcd.print(text[2][i]);  //  Array declared above
        delay(200);
      }
      lcd.noCursor();
      lcd.setCursor(3, 3);  lcd.print("Wait a minute!");
      modem.restart();  //  Initializes modem
      Blynk.begin(BLYNK_AUTH_TOKEN, modem, apn, user, pass);  //  Initiate connection to blynk cloud
      localMode = 0;  //  Used in loop to sync data into Blynk cloud
      break;
    }
  }
  digitalWrite(buzzer, 1);
  delay(1000);
  digitalWrite(buzzer, 0);
  lcd.clear();
  for (byte i = 0; i < 20; i++) { //  Displays scrolling text
    lcd.cursor();
    lcd.print(text[0][i]);  //  Array declared above
    delay(200);
  }
  lcd.setCursor(7, 1);
  for (byte i = 0; i < 6; i++) {
    lcd.cursor();
    lcd.print(text[1][i]);  //  Array declared above
    delay(200);
  }
  lcd.noCursor();
  lcd.setCursor(0, 3);
  if (localMode)  lcd.print("Local Mode Operation");  //  Indicates device is logging data locally to sd card
  else  lcd.print(" Web Mode Operation");  //  Indicates device is logging data online to Blynk console
  delay(1000);
  lcd.clear();
  lcd.print("Enter Admin Passcode");
  Serial.println("Awaiting Passcode input from Admin");
  for (int c = 0; c < 4; c++) passcode[c] = EEPROM.read(c + 5); //  Read stored passcode on EEPROM addresses into an array
  int n = 0;  //  Initialize keypad key counter
  while (1) { //  Keypad prompt for passcode input, infinite until correct passcode is entered
    if (!localMode)  Blynk.run(); //  Maintain blynk connection if device is in online mode
    char key = keypad.getKey();
    if (isdigit(key)) {
      lcd.setCursor(8 + n, 2);  lcd.write(key); //  Display inputted key temporarily
      digitalWrite(buzzer, 1);
      delay(100);
      digitalWrite(buzzer, 0);
      lcd.setCursor(8 + n, 2);  lcd.write('*'); //  Replace inputted key with *
      str1[n] = key;  //  Store inputted key into an array
      n++;  //  Increase key counter for the next loop
    } else if (key == '*') {  //  Clears the key inputted on screen
      strncpy(str1, "", 4);
      n = 0; //  Reset key-storing array and key counter
      lcd.setCursor(8, 2); lcd.print("    "); //  Clear screen of inputted key(s)
    } else if (key == '#') {  //  Checks key with stored passcode
      if ((!strncmp(str1, passcode, 4)) && (n <= 4)) {  //  If passcode entered is correct
        strncpy(str1, "", 4); //  Reset key-storing array
        n = 0; //  Reset keypad key counter
        lcd.clear();
        lcd.setCursor(3, 0); lcd.print("Access Granted");
        lcd.setCursor(0, 2); lcd.print("Welcome as an Admin!");
        Serial.println("Administrator Passcode correct");
        delay(1000);
        lcd.clear();
        break;
      } else {  //  Incorrect passcode is entered
        strncpy(str1, "", 4);
        n = 0;
        lcd.clear();
        digitalWrite(buzzer, 1);
        lcd.setCursor(1, 0); lcd.print("Invalid Admin Code");
        lcd.setCursor(2, 2); lcd.print("Please try again");
        Serial.println("Invalid Administrator Passcode");
        delay(1000);
        digitalWrite(buzzer, 0);
        lcd.clear();
        lcd.print("Enter Admin Passcode");
      }
    }
  }
  SD.begin(53); //  Initialize the sd card if present
  finger.begin(57600);  //  Initialize the fingerprint sensor and set baud rate
  batteryMeter(); //  Check system battery level for logging
}

void loop() {
  //  Keys A, B, C & D carry out their individual actions
  lcd.setCursor(0, 0); lcd.print("A - New Registration"); //  Key 'A' starts a new user registration
  lcd.setCursor(0, 1); lcd.print("B - Start a Election"); //  Key 'B' starts a the voting process
  lcd.setCursor(0, 2); lcd.print("C - Last Elec.Result"); //  Key 'C' displays the last election result
  lcd.setCursor(0, 3); lcd.print("D - Goto Admin Panel"); //  Key 'D' brings up the administrative section
  if (!localMode) Blynk.run();  //  If in online mode (i.e. not in local mode), maintain connection with Blynk server
  char key = keypad.getKey(); //  Loop awaits input from keypad to prompt an action
  switch (key) {
    case 'A': //  Key 'A' starts a new user registration
      Serial.println("New fingerprint registration");
      lcd.clear();
      if (!localMode) { //  Action should be carried out if device mode is "online"
        Blynk.virtualWrite(V4, 1);  //  Trigger a variable value on Blynk that indicates a new user is being registered
      }
      if (usersid <= 125) { //  AS608 module limit is around 125 users i.e. (number of users <= 125)
        while (!getFingerprintEnroll());  //  Function "getFingerprintEnroll()" enrolls a new user
      }
      if (!localMode) {
        Blynk.virtualWrite(V4, 0);  //  Trigger a variable value on Blynk that indicates new user has been registered
      }
      break;
    case 'B': //  Key 'B' starts a the voting process
      lcd.clear();
      if (!localMode) {
        Blynk.virtualWrite(V4, 2);  //  Trigger a variable value on Blynk that indicates the start of an election process
      }
      runElection();  //  Function "runElection()" kickstarts a new voting process
      if (!localMode) {
        Blynk.virtualWrite(V4, 0);  //  Trigger a variable value on Blynk that indicates the end of an election process
      }
      break;
    case 'C': //  Key 'C' displays the last election result
      lcd.clear();
      if (!localMode) {
        Blynk.virtualWrite(V4, 3);  //  Trigger a variable value on Blynk that indicates the election result is currently displayed
      }
      electionResults();  //  Displays the result of the last concluded election
      if (!localMode) {
        Blynk.virtualWrite(V4, 0);  //  Trigger a variable value on Blynk that indicates the election result was recently displayed
      }
      break;
    case 'D': //  Key D brings up the administrative section
      lcd.clear();
      if (!localMode) {
        Blynk.virtualWrite(V4, 4);  //  Trigger a variable value on Blynk that indicates user is currently in administrative section
      }
      admin_panel();  //  Takes user to an administration section
      if (!localMode) {
        Blynk.virtualWrite(V4, 0);  //  Trigger a variable value on Blynk that indicates user just did an administrative task
      }
      break;
  }
}

void batteryMeter() { //  Evaluates system's battery voltage
  double batteryRead = analogRead(battPin); //  Battery +ve terminal is connected to analog pin "battPin"
  /*  Maps battery voltage (batteryRead) to a level between 0 - 100%
     3.7V is equivalent to 610 bits and denotes 0%
     4.2V is equivalent to 900 bits and denotes 100%
  */
  batteryPercent = map(batteryRead, 610, 900, 0, 100);
  if (batteryPercent < 0)  {
    batteryPercent = batteryPercent * -1;  //  BatteryPercent must be a +ve value
  }
  if (!localMode) {
    Blynk.virtualWrite(V5, batteryPercent); //  Syncs battery percentage to Blynk cloud
  }
}

void admin_panel() {  //  System Administration section
  lcd.print("   Battery - ");
  lcd.print(batteryPercent);  lcd.print("%");
  lcd.setCursor(0, 1);  lcd.print("Enter Admin Passcode");
  int n = 0;  //  Initiates a keypad key counter
  while (1) { //  Stay in the administration section infinitely until a breaking action occurs
    if (!localMode) Blynk.run();  //  Maintain connection with Blynk server
    char key = keypad.getKey();
    if (isdigit(key)) {
      lcd.setCursor(8 + n, 3);  lcd.write(key); //  Display inputted key for few seconds
      digitalWrite(buzzer, 1);
      delay(100);
      digitalWrite(buzzer, 0);
      lcd.setCursor(8 + n, 3);  lcd.write('*'); //  Replaces inputted key with *
      str1[n] = key;  //  Store inputted key into an array
      n++;  //  Increase key counter
    } else if (key == 'C') {  //  Cancels the operation
      strncpy(str1, "", 4); //  Empty the string array
      n = 0;  //  Reset key counter
      lcd.clear();  //  Clear screen
      return; //  Return to main-menu screen
    } else if (key == '*') {  //  Clears inputted keys on screen
      strncpy(str1, "", 4);
      n = 0;
      lcd.setCursor(8, 3); lcd.print("    ");
    } else if (key == '#') {  //  Checks for passcode authentication
      lcd.clear();
      if ((!strncmp(str1, passcode, 4)) && (n <= 4)) {  //  If passcode is valid
        strncpy(str1, "", 4);
        n = 0;
        lcd.clear();
        lcd.print("   Welcome, Admin   ");  //  Welcomes Admin and display list of possible actions
        lcd.setCursor(0, 1); lcd.print("A: Check Reg. Voters");
        lcd.setCursor(0, 2); lcd.print("B: Change Admin Code");
        lcd.setCursor(0, 3); lcd.print("D: Delete All Voters");
        while (1) { //  Stays at Admin section infinitely until breaking-action is triggered
          if (!localMode) Blynk.run();  //  Maintain Blynk server connection in infinite loop
          char key = keypad.getKey();
          switch (key) {
            case 'A': //  Key 'A' displays the number of registered voters
              lcd.clear();  lcd.print("  Total number of");
              lcd.setCursor(0, 1);  lcd.print("Registered Voters = ");
              lcd.setCursor(9, 3);  lcd.print(usersid - 1);
              delay(2000);
              return; //  Return to main-screen
            case 'B': //  Key 'B' initiates passcode change
              lcd.clear();
              lcd.setCursor(1, 1);  lcd.print("Enter New Passcode");
              while (n < 5) { //  Passcode can't be more than 4 digits
                if (!localMode) Blynk.run();
                char key = keypad.getKey();
                if (isdigit(key)) {
                  lcd.setCursor(8 + n, 3);  lcd.write(key);
                  digitalWrite(buzzer, 1);
                  delay(100);
                  digitalWrite(buzzer, 0);
                  lcd.setCursor(8 + n, 3);  lcd.write('*');
                  str1[n] = key;
                  n++;
                } else if (key == 'C') {  //  Cancel the operation
                  strncpy(str1, "", 4);
                  n = 0;
                  lcd.clear();
                  return; //  Return to main-menu screen
                } else if (key == '*') {  //  Clear inputted digits on screen
                  strncpy(str1, "", 4);
                  n = 0;
                  lcd.setCursor(8, 3);  lcd.print("    ");
                } else if ((key == '#') && (n == 4)) {  //  Change passcode with new string of digits
                  for (int i = 0; i < 4; i++)  EEPROM.update(i + 5, str1[i]); //  Update EEPROM addresses with inputted key (stored in an array)
                  for (int c = 0; c < 4; c++) passcode[c] = EEPROM.read(c + 5); //  Retrieve the new passcode from EEPROM
                  lcd.clear();
                  lcd.print(" Admin Code changed");
                  lcd.setCursor(4, 2);  lcd.print("Successfully");
                  delay(2000);
                  return; //  passcode changed succesfully, return to main-screen
                }
              }
              break;  //  Break from switch case
            case 'C': //  Key 'C' clears screen and return to main-menu
              lcd.clear();
              return;
            case 'D': //  Key 'D' deletes all registered voters/users
              finger.emptyDatabase(); //  Deletes the database on the fingerprint module
              /*  Resets the number of registered users to 1 and not to 0 because
                user registration on the fingerprint module must always start on index 1
              */
              usersid = 1;  //  Resets the number of registered users to 1
              EEPROM.update(0, usersid);  //  Update the EEPROM with the new number of registered users (usersid)
              lcd.clear();
              lcd.setCursor(0, 1);  lcd.print(" All Users deleted");
              lcd.setCursor(4, 3);  lcd.print("successfully");
              if (!localMode) Blynk.virtualWrite(V0, 0);  //  Update the variable on Blynk cloud
              SD.remove("RegisteredUsers.txt"); //  Deletes the file of registered voters on SD card
              delay(2000);
              return; //  Returns to main-screen
          }
        }
      } else {
        strncpy(str1, "", 4);
        n = 0;
        lcd.clear();
        lcd.setCursor(1, 1);  lcd.print("Invalid Admin Code");
        digitalWrite(buzzer, 1);
        delay(1000);
        digitalWrite(buzzer, 0);
        lcd.clear();
        return;
      }
    }
  }
}

void electionResults() {  //  Displays the result of recently concluded election
  lcd.home(); lcd.print("  Total Voters = ");
  lcd.print(a_count + b_count + c_count + d_count); //  Add up all individual votes from candidates
  lcd.setCursor(4, 1);
  lcd.print("A - ");  lcd.print(a_count); //  Display the votes for Candidate A
  lcd.print(", B - ");  lcd.print(b_count);
  lcd.setCursor(4, 2);
  lcd.print("C - "); lcd.print(c_count);
  lcd.print(", D - ");  lcd.print(d_count);
  if ((a_count > b_count) && (a_count > c_count) && (a_count > d_count)) {  //  If 'A' has maximum number of voters
    lcd.setCursor(1, 3);  lcd.print("Candidate A wins!!!");
  } else if ((b_count > a_count) && (b_count > c_count) && (b_count > d_count)) {  //  If 'B' has maximum number of voters
    lcd.setCursor(1, 3);  lcd.print("Candidate B wins!!!");
  } else if ((c_count > a_count) && (c_count > b_count) && (c_count > d_count)) {  //  If 'C' has maximum number of voters
    lcd.setCursor(1, 3);  lcd.print("Candidate C wins!!!");
  } else if ((d_count > a_count) && (d_count > b_count) && (d_count > c_count)) {  //  If 'D' has maximum number of voters
    lcd.setCursor(1, 3);  lcd.print("Candidate D wins!!!");
  } else if (a_count == b_count == c_count == d_count == 0) { //  No voter voted
    lcd.setCursor(2, 3); lcd.print("No valid Voter!!!");
  } else if (a_count == b_count == c_count == d_count) {  //  Equal number of voters
    lcd.setCursor(2, 3);  lcd.print("Election is a tie");
  }
  /* A bug here is that if 2/3 candidates have same amount of voters
      No condition is explicitly stated to handle the occurrence of these scenarios
  */
  delay(5000);  //  Displays that info for 5 seconds before clearing off the screen
  lcd.clear();
}

void runElection() {  //  Start and run an electoral process
  lcd.home(); lcd.print("   Reg. Voters = ");
  /* Since usersid is always ahead with +1, the real number of registered users
      will be usersid - 1
  */
  lcd.print(usersid - 1); //  Print the real number of usersid
  lcd.setCursor(0, 2);  lcd.print("Enter Admin Passcode");
  int n = 0;  //  Initialize key counter
  while (1) { //  Infinitely loop the authentication process until a break condition is initiated
    if (!localMode) Blynk.run();  //  Maintain connection with Blynk server
    char key = keypad.getKey();
    if (isdigit(key)) { //  If only a digit key is pressed
      lcd.setCursor(8 + n, 3);  lcd.write(key);
      digitalWrite(buzzer, 1);
      delay(100);
      digitalWrite(buzzer, 0);
      lcd.setCursor(8 + n, 3);  lcd.write('*');
      str1[n] = key;
      n++;
    } else if (key == 'C') {  //  Cancel the authentication process
      strncpy(str1, "", 4);
      n = 0;
      lcd.clear();
      return;
    } else if (key == '*') {  //  Clear screen of keys and empty key-array
      strncpy(str1, "", 4);
      n = 0;
      lcd.setCursor(8, 3); lcd.print("    ");
    } else if (key == '#') {  //  Authenticate inputted key
      if ((!strncmp(str1, passcode, 4)) && (n <= 4)) {  //  If passcode entered is correct
        strncpy(str1, "", 4);
        n = 0;
        lcd.clear();
        //  Clear the results of the previous election before starting a new one
        a_count = b_count = c_count = d_count = 0;  //  Resets the count variable of each candidate's voters
        EEPROM.update(1, a_count);  //  Update EEPROM address 1 with new number of voters for candidate A which is 0
        EEPROM.update(2, b_count);  //  Update EEPROM address 2 with new number of voters for candidate B which is 0
        EEPROM.update(3, c_count);  //  Update EEPROM address 3 with new number of voters for candidate C which is 0
        EEPROM.update(4, d_count);  //  Update EEPROM address 4 with new number of voters for candidate D which is 0
        for (int i = 0; i <= 120;  i++) voted_users[i] = 0;  //  Clear and empty the array that holds the IDs of voted users if it exists
        voted_users_count = 0;  //  Reset the variable storing the number of voted users
        if (!localMode) { //  Online mode, sync new data to Blynk Cloud before proceeding
          Blynk.virtualWrite(V3, voted_users_count);
          Blynk.virtualWrite(V1, a_count);
          Blynk.virtualWrite(V2, b_count);
          Blynk.virtualWrite(V6, c_count);
          Blynk.virtualWrite(V7, d_count);
        } //  Previous result clearing process ends here
        lcd.setCursor(3, 1); lcd.print("Access Granted");
        delay(1000);
        lcd.clear();
        while (1) { //  Infinitely loop the election process unless a break is initiated
          if (!localMode) Blynk.run();
ELECTION: //  Electoral process starts here
          lcd.home(); lcd.print("Election in Progress");
          lcd.setCursor(0, 2); lcd.print("Awaiting Fingerscan?");
          int userpass = getFingerprintIDez();  //  Returns fingerprint ID if it's registered in fingerprint module database,
          if (userpass > 0) { //  If it is registered (returns a +ve ID), then:
            lcd.clear();
            for (int i = 1;  i <= 120; i++) { //  Iterate through the array containing the list of already voted users
              if (userpass == voted_users[i]) { //  Compares ID obtained with all the content of the array of voted users. If the ID has voted, then:
                lcd.setCursor(7, 1);  lcd.print("USER-0");
                lcd.print(userpass);
                lcd.setCursor(4, 3);  lcd.print("already voted");
                delay(2000);
                lcd.clear();
                goto ELECTION;  //  Go back up to "ELECTION" label to request new fingerprint for scan
              }
            } //  Passed. Fingerprint ID is registered but hasn't voted
            while (1) {
              if (!localMode) Blynk.run();
              lcd.home();
              lcd.print("  Welcome, USER-0"); lcd.print(userpass);
              lcd.setCursor(0, 2);  lcd.print("Press A, B, C, D, to");
              lcd.setCursor(2, 3);  lcd.print("choose your vote");
              char key = keypad.getKey();
              if (key == 'A') { //  User is voting for candidate A
                a_count++;  //  Increase variable count for candidate A
                EEPROM.update(1, a_count);  //  Update EEPROM with new candidate A count
                lcd.clear();
                lcd.print("  Vote registered");
                lcd.setCursor(4, 2);  lcd.print("Thank you!!!");
                voted_users_count++;  //  Increase the variable count of total users that have voted
                voted_users[voted_users_count] = userpass;  //  Store the ID of current user into voted users array to avoid re-voting.
                File dataFile = SD.open("VotedUsers.txt", FILE_WRITE);  //  Open the file on sd card that logs the list of voted users
                dataFile.println("USER-0" + String(userpass) + " voted successfully for Candidate A");  //  Save the ID on sd card
                dataFile.close(); //  Close the sd card for editing
                if (!localMode) { //  Sync the data to cloud if in online mode
                  Blynk.virtualWrite(V1, a_count);
                  Blynk.virtualWrite(V3, a_count + b_count + c_count + d_count);
                }
                delay(2000);
                lcd.clear();
                goto ELECTION;  //  Go back to "ELECTION" label to request new fingerprint
              } else if (key == 'B') {  //  Do same if Candidate chosen is B
                b_count++;
                EEPROM.update(2, b_count);
                lcd.clear();  lcd.print("  Vote registered");
                lcd.setCursor(4, 2);  lcd.print("Thank you!!!");
                voted_users_count++;
                voted_users[voted_users_count] = userpass;
                File dataFile = SD.open("VotedUsers.txt", FILE_WRITE);
                dataFile.println("USER-0" + String(userpass) + " voted successfully for Candidate B");
                dataFile.close();
                if (!localMode) {
                  Blynk.virtualWrite(V2, b_count);
                  Blynk.virtualWrite(V3, a_count + b_count + c_count + d_count);
                }
                delay(2000);
                lcd.clear();
                goto ELECTION;
              } else if (key == 'C') {  //  Do same if Candidate chosen is C
                c_count++;
                EEPROM.update(3, c_count);
                lcd.clear();  lcd.print("  Vote registered");
                lcd.setCursor(4, 2);  lcd.print("Thank you!!!");
                voted_users_count++;
                voted_users[voted_users_count] = userpass;
                File dataFile = SD.open("VotedUsers.txt", FILE_WRITE);
                dataFile.println("USER-0" + String(userpass) + " voted successfully for Candidate C");
                dataFile.close();
                if (!localMode) {
                  Blynk.virtualWrite(V6, c_count);
                  Blynk.virtualWrite(V3, a_count + b_count + c_count + d_count);
                }
                delay(2000);
                lcd.clear();
                goto ELECTION;
              } else if (key == 'D') {  //  Do same if Candidate chosen is D
                d_count++;
                EEPROM.update(4, d_count);
                lcd.clear();
                lcd.print("  Vote registered");
                lcd.setCursor(4, 2);  lcd.print("Thank you!!!");
                voted_users_count++;
                voted_users[voted_users_count] = userpass;
                File dataFile = SD.open("VotedUsers.txt", FILE_WRITE);
                dataFile.println("USER-0" + String(userpass) + " voted successfully for Candidate D");
                dataFile.close();
                if (!localMode) {
                  Blynk.virtualWrite(V7, c_count);
                  Blynk.virtualWrite(V3, a_count + b_count + c_count + d_count);
                }
                delay(2000);
                lcd.clear();
                goto ELECTION;
              }
            }
          } else if (userpass < 0) {  //  userpass returns -ve value if user isn't registered or invalid print image
            lcd.clear();  lcd.print(" User not registered");
            lcd.setCursor(2, 1);  lcd.print("Please try again");
            lcd.setCursor(1, 3);  lcd.print("Or clean & rescan!");
            delay(2000);
            lcd.clear();
            goto ELECTION;  //  Go back to "ELECTION" label to request new fingerprint
          } else if (userpass == 0) { //  userpass returns 0 when election process is canceled, this requires authorization as well
            lcd.clear();
            lcd.setCursor(4, 0);  lcd.print("End Election?");
            lcd.setCursor(0, 2);  lcd.print("Enter Admin Passcode");
            int n = 0;  //  Initialize keypad key counter
            while (1) { //  Loops infinitely until authorization occurs or user returns back to "ELECTION" label (process)
              if (!localMode) Blynk.run();
              char key = keypad.getKey();
              if (isdigit(key)) {
                lcd.setCursor(8 + n, 3);  lcd.write(key);
                digitalWrite(buzzer, 1);
                delay(100);
                lcd.setCursor(8 + n, 3);  lcd.write('*');
                digitalWrite(buzzer, 0);
                str1[n] = key;
                n++;
              } else if (key == '*') {
                strncpy(str1, "", 4);
                n = 0;
                lcd.setCursor(8, 3); lcd.print("    ");
              } else if (key == 'C') {
                strncpy(str1, "", 4);
                n = 0;
                lcd.clear();
                goto ELECTION;
              } else if (key == '#') {
                if ((!strncmp(str1, passcode, 4)) && (n <= 4)) {  //  Authorization confirmed, then, election ends
                  strncpy(str1, "", 4);
                  n = 0;
                  lcd.clear();
                  //  Logs all data and brief analysis from election
                  File dataFile = SD.open("VotedUsers.txt", FILE_WRITE);
                  dataFile.println("Candidate A has " + String(a_count) + " total votes.");
                  dataFile.println("Candidate B has " + String(b_count) + " total votes.");
                  dataFile.println("Candidate C has " + String(c_count) + " total votes.");
                  dataFile.println("Candidate D has " + String(d_count) + " total votes.");
                  dataFile.println("Total registered users are " + String(usersid - 1) + " users.");
                  dataFile.println("All users that voted are " + String(a_count + b_count + c_count + d_count) + " users.");
                  dataFile.println("Number of registered users that did not vote are " + String(usersid - 1 - a_count - b_count - c_count - d_count) + " users.");
                  dataFile.println("Election ended, Thank you!");
                  dataFile.close();
                  electionResults();
                  return; //  Return to main-screen
                } else {  //  User is unauthorized for stopping election process, thus, election continues
                  strncpy(str1, "", 4);
                  n = 0;  lcd.clear();
                  lcd.print(" Invalid Admin Code");
                  delay(1000);
                  lcd.clear();
                  goto ELECTION;
                }
              }
            }
          }
        }
      } else  {
        strncpy(str1, "", 4);
        n = 0;
        lcd.clear();
        lcd.print(" Invalid Admin Code");
        digitalWrite(buzzer, 1);
        delay(1000);
        digitalWrite(buzzer, 0);
        lcd.clear();
        return;
      }
    }
  }
}

//  Registration of new fingerprint
uint8_t getFingerprintEnroll() {
  int p = -1;
  lcd.setCursor(0, 1); lcd.print("Please scan");
  lcd.setCursor(9, 3);  lcd.print("your finger");
  Serial.println("Awaiting new fingerprint scan");
  while (p != FINGERPRINT_OK) {
    if (!localMode) Blynk.run();
    p = finger.getImage();  //  Loops until fingerprint image is acquired from scanner
    char key = keypad.getKey(); //  Also await keypad response for some actions
    if (key == 'C')  return;  //  Key C cancels the fingerprint checking process and returns 0 to previous embodied function
    switch (p) {
      case FINGERPRINT_OK:  //  A clear image of fingerprint is obtained
        lcd.clear();
        lcd.setCursor(0, 1);  lcd.print("Fingerprint scanned");
        Serial.println("Fingerprint scanned successfully");
        delay(1000);
        break;
    }
  }
  lcd.clear();
  // OK success!
  p = finger.image2Tz(1); //  Fingerprint image is being converted
  switch (p) {
    case FINGERPRINT_OK:  //  Image conversion process was smooth
      break;
    // In the following 3 similar cases, fingerprint conversion wasn't successful, hence, grouped into one
    case FINGERPRINT_IMAGEMESS:
    case FINGERPRINT_INVALIDIMAGE:
    case FINGERPRINT_FEATUREFAIL:
      lcd.setCursor(6, 1);  lcd.print("Failed!");
      lcd.setCursor(1, 3);  lcd.print("Pls clean & rescan!");
      delay(1000);
      lcd.clear();
      return p;
  }
  lcd.setCursor(3, 1);  lcd.print("Remove finger");
  delay(2000);
  lcd.clear();
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    if (!localMode) Blynk.run();
    p = finger.getImage();  //  Recapture fingerprint image
  }
  p = -1;
  lcd.setCursor(3, 1);  lcd.print("Scan same finger");
  //  Proceeds to rescan finger (Fingerprint image dump is taken twice)
  while (p != FINGERPRINT_OK) {
    if (!localMode) Blynk.run();
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:  //  Both fingerprints (1st & 2nd) match
        lcd.clear();
        lcd.setCursor(5, 1);  lcd.print("!Success!");
        lcd.setCursor(0, 3);  lcd.print(" Fingerprint scanned");
        Serial.println("Fingerprints matched successfully");
        delay(1000);
        break;
    }
  }
  lcd.clear();
  // OK success!
  p = finger.image2Tz(2); //  Fingerprint image is being converted
  switch (p) {
    case FINGERPRINT_OK:  //  Image conversion process was smooth
      break;
    case FINGERPRINT_IMAGEMESS:
    case FINGERPRINT_INVALIDIMAGE:
    case FINGERPRINT_FEATUREFAIL:
      lcd.setCursor(6, 1);  lcd.print("Failed!");
      lcd.setCursor(1, 3);  lcd.print("Pls clean & rescan!");
      delay(1000);
      lcd.clear();
      return p;
  }
  // OK converted!
  p = finger.createModel(); //  Creates a model of the fingerprint image
  if (p == FINGERPRINT_OK) {  //  Model creation was successful
    lcd.setCursor(6, 0);  lcd.print("!Success!");
    lcd.setCursor(3, 1);  lcd.print("Prints matched");
    delay(2000);
    lcd.clear();
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {  //  Model creation wasn't successful
    lcd.setCursor(7, 1);  lcd.print("Failed");
    lcd.setCursor(3, 3);  lcd.print("Print mismatch!");
    delay(1000);
    lcd.clear();
    return p;
  }
  /*  //  Inititate storage of the model created
       (fingerprint image) to database on
       index 'usersid'
       i.e. store current usersid e.g. 1 on database index 'userid'
  */
  p = finger.storeModel(usersid);
  if (p == FINGERPRINT_OK) {  //  Fingerprint model stored successfully
    Serial.println("New User with ID: USER-0" + String(usersid) + " registered successfully");
    lcd.setCursor(2, 0);  lcd.print("Your ID: USER-0");
    lcd.print(usersid);
    lcd.setCursor(3, 2);  lcd.print("Biodata stored");
    lcd.setCursor(4, 3);  lcd.print("Successfully");
    File dataFile = SD.open("RegistedUsers.txt", FILE_WRITE);
    dataFile.println("USER-0" + String(usersid) + " biodata is successfully registered.");
    dataFile.close();
    usersid++;  //  Increase usersid for event of next fingerprint enrollment
    EEPROM.update(0, usersid);  //  Update EEPROM with new value of usersid
    delay(3000);
    lcd.clear();
    if (!localMode) Blynk.virtualWrite(V0, usersid - 1);  //  Update Blynk cloud with new value of usersid
  }
  return true;
}
/*  Check a fingerprint against the database
    Return +ve value index of the fingerprint ID if registered
    Returns -1 or -ve value if fingerprint is unregistered
*/
int getFingerprintIDez() {
  int p = -1;
  while (p != FINGERPRINT_OK) {
    if (!localMode) Blynk.run();
    p = finger.getImage();
    char key = keypad.getKey();
    if (key == 'C')  return 0;
    switch (p) {
      case FINGERPRINT_OK:
        lcd.clear();
        lcd.setCursor(0, 1);  lcd.print("Fingerprint scanned");
        delay(1000);
        break;
    }
  }
  lcd.clear();
  // OK success!
  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      break;
    case FINGERPRINT_IMAGEMESS:
    case FINGERPRINT_INVALIDIMAGE:
    case FINGERPRINT_FEATUREFAIL:
      lcd.clear();
      return -1;
  }
  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK)  {
    return -1;
  }
  lcd.clear();
  return finger.fingerID;
}
