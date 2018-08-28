enum gameStates {SETUP, GAME, DEATH};
enum moleStates {HIDDEN, ABOVE};
byte gameState = SETUP;
byte moleState = HIDDEN;
long gameBeginTime;
long timeSinceGameBegan;
int waitIncrement;
int countdownIncrement;
byte countdownState = 0;
Timer waitTimer;
Timer countdownTimer;
void setup() {
  // put your setup code here, to run once:
  setColor(ORANGE);
}
void loop() {
  // FIX RANDOMNESS
  rand(1);
  switch (gameState) {
    case SETUP:
      setupLoop();
      break;
    case GAME:
      gameLoop();
      break;
    case DEATH:
      deathLoop();
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
    setColor(GREEN);
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
  //now that we've evaluated everything, let's do the visual transition
  if (gameState == DEATH) {
    setColor(RED);
  }
}
void hiddenMoleLoop() {
  //here we wait for the timer to expired
  if (waitTimer.isExpired()) {
    waitIncrement = 3000 - (timeSinceGameBegan / 30);
    countdownIncrement = waitIncrement / 3;
    countdownTimer.set(countdownIncrement);
    moleState = ABOVE;
    setColor(ORANGE);
  }
}
void aboveMoleLoop() {
  //first, we listen for the countdown timer to expire
  if (countdownTimer.isExpired()) {
    countdownState++;
    countdownTimer.set(countdownIncrement);
    setColorOnFace(RED, countdownState);//turn the face red
  }
  //check if the timer has reached 6
  if (countdownState == 6) { //the game has ended, you DUMMY
    gameState = DEATH;
    countdownState = 0;
    moleState = HIDDEN;
  }
  //now that we've done all that, check for clicks
  if (buttonSingleClicked()) {
    moleState = HIDDEN;
    waitIncrement = 3000 - (timeSinceGameBegan / 30);
    waitTimer.set(rand(waitIncrement) + waitIncrement);
    setColor(GREEN);
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
  //now that we've evaluated each transition, let's do the visual transition
  if (gameState == SETUP) {
    setColor(ORANGE);
  }
}

