enum gameStates {SETUP, GAME, DEATH};//cycles through the game
byte gameState = SETUP;

byte playerCount = 1;//only communicated in setup state, ranges from 1-3

byte grassHue = 70;

#define ROUND_MIN 1 //starting round
#define ROUND_MAX 20 //final difficulty level, though the game continues
byte roundCounter = 0;
Timer roundTimer;
bool roundActive = false;

enum goSignals {INERT, GO, RESOLVING};//used in game state to signal round begin from the master mole
byte goSignal = INERT;
bool isRippling = false;
byte ripplingInterval = 200;
Timer ripplingTimer;

byte currentPlayerMole = 1;
byte playerMoleUsage[3] = {0, 0, 0};
Color playerColors[3] = {makeColorHSB(0, 255, 255), makeColorHSB(42, 255, 255), makeColorHSB(212, 255, 255)};

#define EMERGE_INTERVAL_MAX 1500
#define EMERGE_INTERVAL_MIN 750
#define EMERGE_DRIFT 1000
Timer emergeTimer;//triggered when the GO signal is received, interval shrinks as roundCounter increases

#define POP_CHANCE_MAX 80
#define POP_CHANCE_MIN 50

bool isAbove = false;
#define ABOVE_INTERVAL_MAX 2000
#define ABOVE_INTERVAL_MIN 1000
Timer aboveTimer;

bool isFlashing = false;
int flashingInterval = 500;
Timer flashingTimer;

bool isStriking = false;
byte strikingInterval = 200;
Timer strikingTimer;
byte strikes = 0;//communicated in game mode, incremented which each strike
Color strikeColors[3] = {YELLOW, ORANGE, RED};

bool isSourceOfDeath;
byte losingPlayer = 0;

void setup() {
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:
  switch (gameState) {
    case SETUP:
      setupLoop();
      setupDisplayLoop();
      break;
    case GAME:
      gameLoopGeneric();
      gameDisplayLoop();
      break;
    case DEATH:
      deathLoop();
      deathDisplayLoop();
      break;
  }

  //dump button data
  buttonSingleClicked();
  buttonDoubleClicked();
  buttonPressed();

  //do communication (done here to avoid miscommunication)
  byte sendData;
  switch (gameState) {
    case SETUP:
      sendData = (gameState << 4) + (playerCount);
      break;
    case GAME:
      sendData = (gameState << 4) + (goSignal << 2) + (strikes);
      break;
    case DEATH:
      sendData = (gameState << 4) + (losingPlayer);
      break;
  }
  setValueSentOnAllFaces(sendData);
}

//////////////
//GAME LOOPS//
//////////////

void setupLoop() {
  //listen for clicks to increment player count
  if (buttonSingleClicked()) {
    playerCount++;
    if (playerCount > 3) {
      playerCount = 1;
    }
  }

  //listen for neighbors with higher player counts and conform
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {//a neighbor!
      byte neighborPlayerCount = getPlayerCount(getLastValueReceivedOnFace(f));
      byte neighborGameState = getGameState(getLastValueReceivedOnFace(f));
      if (neighborGameState == SETUP) { //this neighbor is in our mode, so we can trust his communication
        if (playerCount == 1 && neighborPlayerCount == 2) {
          playerCount = 2;
        } else if (playerCount == 2 && neighborPlayerCount == 3) {
          playerCount = 3;
        } else if (playerCount == 3 && neighborPlayerCount == 1) {
          playerCount = 1;
        }
      }
    }
  }

  //listen for double-clicks to move into game mode and become master
  if (buttonDoubleClicked()) {
    gameState = GAME;
    roundActive = false;
    roundTimer.set(EMERGE_INTERVAL_MAX);
    isFlashing = true;
    flashingTimer.set(flashingInterval);
  }

  //listen for neighbors in game mode to move to game mode myself and become receiver
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {//a neighbor!
      byte neighborGameState = getGameState(getLastValueReceivedOnFace(f));
      if (neighborGameState == GAME) {
        gameState = GAME;
        roundActive = false;
        roundTimer.set(EMERGE_INTERVAL_MAX);
        isFlashing = true;
        flashingTimer.set(flashingInterval);
      }
    }
  }
}

void gameLoopGeneric() {

  //start new round based on two different things
  if (!roundActive) {//so a round is not started
    bool newRoundInitiated = false;


    //look for neighbors commanding us to start a round
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) { //neighbor!
        if (getGameState(getLastValueReceivedOnFace(f)) == GAME) { //a trusted neighbor
          if (getGoSignal(getLastValueReceivedOnFace(f)) == GO) {//telling us to go
            newRoundInitiated = true;
          }
        }
      }
    }

    //listen for internal timer telling us to go
    if (!roundActive && roundTimer.isExpired()) {
      newRoundInitiated = true;
    }

    //start a new round!
    if (newRoundInitiated) {
      if (roundCounter < ROUND_MAX) {
        roundCounter++;
      }

      isRippling = true;
      ripplingTimer.set(ripplingInterval);
      goSignal = GO;
      roundActive = true;

      int emergeInterval = map_m(roundCounter, ROUND_MIN, ROUND_MAX, EMERGE_INTERVAL_MAX, EMERGE_INTERVAL_MIN);
      emergeTimer.set(emergeInterval + rand(EMERGE_DRIFT));
      int aboveInterval = map_m(roundCounter, ROUND_MIN, ROUND_MAX, ABOVE_INTERVAL_MAX, ABOVE_INTERVAL_MIN);

      int roundInterval = emergeInterval + EMERGE_DRIFT + aboveInterval + flashingInterval + emergeInterval;
      roundTimer.set(roundInterval);
    }
  }

  //resolve goSignal propogation
  if (goSignal == GO) {//we are going. Do all our neighbors know this?
    bool canResolve = true;

    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {//neighbor!
        if (getGoSignal(getLastValueReceivedOnFace(f)) == INERT) {//this neighbor has not been told
          canResolve = false;
        }
      }
    }

    if (canResolve) {
      goSignal = RESOLVING;
    }
  } else if (goSignal == RESOLVING) {
    bool canInert = true;

    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {//neighbor!
        if (getGoSignal(getLastValueReceivedOnFace(f)) == GO) {//this neighbor still has work to do
          canInert = false;
        }
      }
    }

    if (canInert) {
      goSignal = INERT;
    }
  }

  //listen for my emerge timer to expire so I can go above
  if (roundActive && emergeTimer.isExpired()) {
    roundActive = false;
    //calculate if I should go above
    byte popChance = map_m(roundCounter, ROUND_MIN, ROUND_MAX, POP_CHANCE_MIN, POP_CHANCE_MAX);
    if (rand(100) < popChance) {
      isAbove = true;
      int fadeTime = map_m(roundCounter, ROUND_MIN, ROUND_MAX, ABOVE_INTERVAL_MAX, ABOVE_INTERVAL_MIN);
      aboveTimer.set(fadeTime);
      //set which player is up
      if (playerCount > 1) {//multiplayer
        //choose a mole that has been used less than twice since the last reset
        do {
          currentPlayerMole = rand(playerCount - 1) + 1;
        } while (playerMoleUsage[currentPlayerMole - 1] == 2);
        //we found one! increment that placement
        playerMoleUsage[currentPlayerMole - 1] += 1;

        //if all moles have been used twice, do a reset
        if ((playerMoleUsage[0] + playerMoleUsage[1] + playerMoleUsage[2]) == playerCount * 2) {
          playerMoleUsage[0] = 0;
          playerMoleUsage[1] = 0;
          playerMoleUsage[2] = 0;
        }
      } else {//singleplayer
        currentPlayerMole = 1;
      }
    }
  }

  //listen for button presses
  if (buttonPressed()) {
    if (isAbove) { //there is a mole here
      isAbove = false;//kill the mole
      isFlashing = true;//start the flash
      flashingTimer.set(flashingInterval);
      roundActive = false;
    } else {//there is no mole here
      if (playerCount == 1) {//single player, get a strike
        strikes++;
        strikingTimer.set(strikingInterval);
        isStriking = true;
        if (strikes == 3) {
          gameState = DEATH;
          isSourceOfDeath = true;
          losingPlayer = 1;
        }
      } else {//just ripple it a bit to show we heard you
        isRippling = true;
        ripplingTimer.set(ripplingInterval);
      }
    }
  }//end button press check

  //listen for strikes from neighbors
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) { //neighbor!
      byte neighborStrikes = getStrikes(getLastValueReceivedOnFace(f));
      byte neighborGameState = getGameState(getLastValueReceivedOnFace(f));
      if (neighborGameState == GAME) { //this neighbor is in game state, so we can trust their communication
        if (neighborStrikes > strikes) { //that neighbor is reporting more strikes than me. Take that number
          strikes = neighborStrikes;
          isStriking = true;
          strikingTimer.set(strikingInterval);
        }
      }
    }
  }

  //listen for my mole to cause death
  if (isAbove && aboveTimer.isExpired()) { //my fade timer expired and I haven't been clicked, so...
    gameState = DEATH;
    isSourceOfDeath = true;
    losingPlayer = currentPlayerMole;
  }

  //listen for death
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {//a neighbor!
      byte neighborGameState = getGameState(getLastValueReceivedOnFace(f));
      if (neighborGameState == DEATH) {
        gameState = DEATH;
        isSourceOfDeath = false;
      }
    }
  }//end death check
}

void deathLoop() {

  //listen for losing player
  if (!isSourceOfDeath && losingPlayer == 0) {//I am not the source of death, and I don't yet know who lost
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) { //neighbor!
        byte neighborLosingPlayer = getLosingPlayer(getLastValueReceivedOnFace(f));
        byte neighborGameState = getGameState(getLastValueReceivedOnFace(f));
        if (neighborGameState == DEATH) { //this neighbor is in death state, so we can trust their communication
          if (neighborLosingPlayer != 0) {//this neighbor seems to know who lost
            losingPlayer = neighborLosingPlayer;
          }
        }
      }
    }
  }

  //listen for double clicks to go back to setup
  if (buttonDoubleClicked()) {
    gameState = SETUP;
    resetAllVariables();
  }

  //listen for signal to go to setup
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {//a neighbor!
      byte neighborGameState = getGameState(getLastValueReceivedOnFace(f));
      if (neighborGameState == SETUP) {
        gameState = SETUP;
        resetAllVariables();
      }
    }
  }//end setup check
}

void resetAllVariables() {
  //RESET ALL GAME VARIABLES
  playerCount = 1;
  goSignal = INERT;
  roundCounter = 0;
  roundActive = false;
  currentPlayerMole = 0;
  losingPlayer = 0;
  strikes = 0;
  isSourceOfDeath = false;
  isAbove = false;
  isFlashing = false;
  isRippling = false;
  isStriking = false;
  playerMoleUsage[0] = 0;
  playerMoleUsage[1] = 0;
  playerMoleUsage[2] = 0;
}

/////////////////
//DISPLAY LOOPS//
/////////////////

void setupDisplayLoop() {
  setColor(makeColorHSB(grassHue, 255, 255));

  setColorOnFace(playerColors[0], 0);//we always have player 1

  if (playerCount >= 2) {//do we have player 2?
    setColorOnFace(playerColors[1], 2);
  }

  if (playerCount == 3) {//do we have player 3?
    setColorOnFace(playerColors[2], 4);
  }
}

void gameDisplayLoop() {
  //do each animation
  if (isFlashing) {//fade from white to green based on flashingTimer
    byte currentSaturation = 255 - map_m(flashingTimer.getRemaining(), 0, flashingInterval, 0, 255);
    setColor(makeColorHSB(grassHue, currentSaturation, 255));
  } else if (isAbove) {//fade from [color] to off based on aboveTimer
    long currentInterval = map_m(roundCounter, ROUND_MIN, ROUND_MAX, ABOVE_INTERVAL_MAX, ABOVE_INTERVAL_MIN);
    long currentTime = aboveTimer.getRemaining();
    byte brightnessSubtraction = map_m(currentTime, currentInterval, 0, 0, 255);
    brightnessSubtraction = (brightnessSubtraction * brightnessSubtraction) / 255;
    brightnessSubtraction = (brightnessSubtraction * brightnessSubtraction) / 255;
    byte currentBrightness = 255 - brightnessSubtraction;
    Color currentColor = playerColors[currentPlayerMole - 1];
    setColor(dim(currentColor, currentBrightness));
  } else if (isStriking) {//flash [color] for a moment
    //which color? depends on number of strikes
    setColor(strikeColors[strikes - 1]);
  } else if (isRippling) {//randomize green hue for a moment
    FOREACH_FACE(f) {
      setColorOnFace(makeColorHSB(grassHue, 255, rand(50) + 205), f);
      //setColorOnFace(makeColorHSB(grassHue + rand(20), 255, 255), f);
    }
  } else {//just be green
    setColor(makeColorHSB(grassHue, 255, 255));
  }

  //resolve non-death animation timers
  if (flashingTimer.isExpired()) {
    isFlashing = false;
  }
  if (strikingTimer.isExpired()) {
    isStriking = false;
  }
  if (ripplingTimer.isExpired()) {
    isRippling = false;
  }
}

void deathDisplayLoop() {
  setColor(OFF);
  if (isSourceOfDeath) {
    setColor(WHITE);
  } else {
    setColor(playerColors[losingPlayer - 1]);
  }
}

/////////////////
//COMMUNICATION//
/////////////////

byte getGameState(byte data) {//1st and 2nd bit
  return (data >> 4);
}

byte getPlayerCount(byte data) {//5th and 6th bit
  return (data & 3);
}

byte getGoSignal(byte data) {//3rd and 4th bit
  return ((data >> 2) & 3);
}

byte getStrikes(byte data) {//5th and 6th bit
  return (data & 3);
}

byte getLosingPlayer(byte data) {//5th and 6th bit
  return (data & 3);
}

///////////////
//CONVENIENCE//
///////////////

long map_m(long x, long in_min, long in_max, long out_min, long out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
