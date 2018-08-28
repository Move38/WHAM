/*
   Wackamole

   written by:
   Dan King

   updated by:
   Jonathan Bobrow
*/
enum gameStates {SETUP, GAME, DEATH};
enum moleStates {HIDDEN, ABOVE};

byte gameState = SETUP;
byte moleState = HIDDEN;

long gameBeginTime;
long timeSinceGameBegan;

long timeOfWhack;

int waitIncrement;
int countdownIncrement;

byte countdownState = 0;

Timer waitTimer;
Timer countdownTimer;

bool isSourceOfDeath = false;

void setup() {
  // put your setup code here, to run once:
}

void loop() {
  rand(1);      // make random seem actually random... this should not be necessary

  switch (gameState) {
    case SETUP:
      setupLoop();
      displaySetup();
      break;
    case GAME:
      gameLoop();
      displayGame();
      break;
    case DEATH:
      deathLoop();
      displayDeath();
      break;
  }

  //send out communications
  byte sendData = (gameState * 10) + moleState;
  setValueSentOnAllFaces(sendData);
}

void setupLoop() {
  //look for double clicks to move to the next state
  if (buttonDoubleClicked()) {
    gameState = GAME;
    isSourceOfDeath = false;
  }
  //also look for neighbors in game state and go to game
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {//found a neighbor
      byte neighborData = getLastValueReceivedOnFace(f);
      if (neighborData / 10 == GAME) {//neighbor is in game state. GO!
        gameState = GAME;
      }
    }
  }
  //now that we've evaluated each transition, let's do the visual transition
  if (gameState == GAME) {
    gameBeginTime = millis();
    waitTimer.set(rand(5000) + 1000);
  }
}

void gameLoop() {
  //update the time
  timeSinceGameBegan = millis() - gameBeginTime;
  if (timeSinceGameBegan > 60000) { //game has exceeded one minute
    timeSinceGameBegan = 60000;//this stops the game from accelerating further
  }
  //determine which mole loop to run
  if (moleState == HIDDEN) {
    hiddenMoleLoop();
  } else if (moleState == ABOVE) {
    aboveMoleLoop();
  }
  //now, we look for neighbors who have died
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {//found a neighbor
      byte neighborData = getLastValueReceivedOnFace(f);
      if (neighborData / 10 == DEATH) {//neighbor is in DEATH state. GO!
        gameState = DEATH;
        moleState = HIDDEN;
      }
    }
  }
}

void hiddenMoleLoop() {
  //here we wait for the timer to expired
  if (waitTimer.isExpired()) {
    waitIncrement = 3000 - (timeSinceGameBegan / 30);  // a value between 1sec and 3sec, gets shorter as the game goes on
    countdownIncrement = waitIncrement / 3;            // our countdown timer shortens as the game progresses
    countdownTimer.set(countdownIncrement);            // set the countdown timer
    moleState = ABOVE;                                 // we've waited long enough Punxsutawney!
  }

  if (buttonDown()) {
    // miss the mole.. no mole here
  }
}

void aboveMoleLoop() {
  //first, we listen for the countdown timer to expire
  if (countdownTimer.isExpired()) {
    countdownState++;                                   //
    countdownTimer.set(countdownIncrement);             //
  }

  //check if the timer has reached 6
  if (countdownState == 6) { //the game has ended, you DUMMY
    isSourceOfDeath = true;
    gameState = DEATH;
    countdownState = 0;
    moleState = HIDDEN;
  }

  //now that we've done all that, check to see if the user hit the mole!
  if (buttonDown()) {
    timeOfWhack = millis();
    moleState = HIDDEN;
    waitIncrement = 3000 - (timeSinceGameBegan / 30);   // a value between 1sec and 3sec, gets shorter as the game goes on
    waitTimer.set(rand(waitIncrement) + waitIncrement); // a value between our wait time and possibly twice as long
    countdownState = 0;
  }
}

void deathLoop() {
  //look for double clicks to move to the next state
  if (buttonDoubleClicked()) {
    gameState = SETUP;
  }
  //also look for neighbors in game state and go to game
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {//found a neighbor
      byte neighborData = getLastValueReceivedOnFace(f);
      if (neighborData / 10 == SETUP) {//neighbor is in game state. GO!
        gameState = SETUP;
      }
    }
  }
}

/*
   Display states
*/

void displaySetup() {
  setColor(BLUE);
}

void displayGame() {
  //  setColor(GREEN);
  // if we are in hiding, then we should be green
  if (moleState == HIDDEN) {
    setColor(GREEN);
  }
  // if we are showing then we should show red for how long we are showing
  else if (moleState == ABOVE) {
    setColor(ORANGE);
    // show the length of time left
    FOREACH_FACE(f) {
      if ( f < countdownState ) {
        setFaceColor(f, RED);
      }
    }
  }

  // if we get hit, we should flash white...
  long timeSinceWhack = millis() - timeOfWhack;
  if (timeSinceWhack < 1000 ) {
    byte bri = 255 - (timeSinceWhack / 4);
    setColor(dim(WHITE, bri));
  }
}

void displayDeath() {
  if (isSourceOfDeath) {
    //    // saturation oscillate between red and white
    //    byte sat = 127 + (127 * sin_d(millis() / 3));
    //    setColor(makeColorHSB(0, sat, 255));

        // color oscillate between red and yellow
        byte hue = 20 + (20 * sin_d(millis() / 3));
        setColor(makeColorHSB(hue, 255, 255));
  }

  else {
    setColor(RED);
  }
}

/*
   Sin in degrees ( standard sin() takes radians )
*/

float sin_d( uint16_t degrees ) {

  return sin( ( degrees / 360.0F ) * 2.0F * PI   );
}



