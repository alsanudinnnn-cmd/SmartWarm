#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Arduino.h>

#define WIFI_SSID "skibidiisigma"
#define WIFI_PASSWORD "11111112"

#define API_KEY "AIzaSyDlAlFqcoLSTjGFFobyAaPoPlkAykgc0mY"
#define DATABASE_URL "https://smartwarm-a4d71-default-rtdb.asia-southeast1.firebasedatabase.app/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ================= PIN DEFINITIONS =================
#define BUTTON_TOP_LEFT   12
#define BUTTON_TOP_RIGHT  25
#define BUTTON_BOTTOM_LEFT 27
#define BUTTON_BOTTOM_RIGHT 4

#define LED_TOP_LEFT    13
#define LED_TOP_RIGHT   23
#define LED_BOTTOM_LEFT 14
#define LED_BOTTOM_RIGHT 16

#define START_BUTTON 15
#define START_BUTTON_LED 2
#define BUZZER_PIN   17

// ===== NOTE DEFINITIONS =====
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_G5  784

// ================= STAGE SETTINGS =================
const byte TOTAL_STAGES = 3;

const byte STAGE_TARGET_PHASE[TOTAL_STAGES] = {
  5,   // Stage 1
  8,   // Stage 2
  10   // Stage 3
};

// Stage-scaled scoring: harder stages reward more points
const int BASE_SCORES[3]    = { 150, 200, 280 };
const int STREAK_BONUSES[3] = { 300, 400, 500 };

// ================= GAME VARIABLES =================
byte         currentStage = 1;
unsigned long currentPhase = 0;
byte         activeButton = 0;
unsigned int currentStreak = 0;
unsigned int bestStreak    = 0;
unsigned long lastCorrectPressTime = 0;
unsigned int currentScore  = 0;

unsigned long lastButtonPressTime = 0;
const unsigned long BUTTON_COOLDOWN = 300;   // ms
const unsigned long STREAK_WINDOW   = 2500;  // max gap between hits to keep streak

bool isGameRunning = false;
bool startButtonLedState = false;

enum StartButtonBlinkMode {
  START_BLINK_OFF,
  START_BLINK_READY,
  START_BLINK_POWER_READY
};

StartButtonBlinkMode startButtonBlinkMode = START_BLINK_OFF;
unsigned long lastStartButtonBlinkAt = 0;

// -- Ready LED chase animation (TL > TR > BR > BL loop) --
byte  readyAnimStep = 0;          // 0=TL, 1=TR, 2=BR, 3=BL
unsigned long lastReadyAnimAt = 0;
const unsigned long READY_ANIM_INTERVAL = 150; // ms per step
const byte READY_ANIM_PINS[4] = { LED_TOP_LEFT, LED_TOP_RIGHT, LED_BOTTOM_RIGHT, LED_BOTTOM_LEFT };

unsigned long startTime   = 0;
unsigned long elapsedTime = 0;

unsigned long ledTurnedOnTime = 0;
unsigned long totalReactionTime = 0;
int totalHits = 0;

// ── Super power state ──
bool   hasSuperPower        = false;
String currentSuperPowerType = "";
int    pendingTimeCutSeconds = 0;
int    appliedTimeCutSeconds = 0;
bool   usedDoublePoints      = false;
bool   usedOnePointFivePoints = false;
bool   shieldActive          = false;
bool   usedShield            = false;
int    speedBoostHitsLeft    = 0;
bool   usedSpeedBoost        = false;

const int TIME_CUT_SECONDS = 5;
const int SPEED_BOOST_HITS  = 5;

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  randomSeed(micros());

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  config.api_key    = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase signUp OK");
  } else {
    Serial.printf("SignUp Error: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  pinMode(BUTTON_TOP_LEFT,    INPUT_PULLUP);
  pinMode(BUTTON_TOP_RIGHT,   INPUT_PULLUP);
  pinMode(BUTTON_BOTTOM_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_BOTTOM_RIGHT,INPUT_PULLUP);

  pinMode(LED_TOP_LEFT,    OUTPUT);
  pinMode(LED_TOP_RIGHT,   OUTPUT);
  pinMode(LED_BOTTOM_LEFT, OUTPUT);
  pinMode(LED_BOTTOM_RIGHT,OUTPUT);

  pinMode(START_BUTTON, INPUT_PULLUP);
  pinMode(START_BUTTON_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  resetLEDs();
  digitalWrite(START_BUTTON_LED, LOW);
  startButtonBlinkMode = START_BLINK_READY;
  Serial.println("LED Button Game Ready");
  Serial.println("Press Start Button to begin");
}

// ================= HELPER FUNCTIONS =================
void resetLEDs() {
  digitalWrite(LED_TOP_LEFT,    LOW);
  digitalWrite(LED_TOP_RIGHT,   LOW);
  digitalWrite(LED_BOTTOM_LEFT, LOW);
  digitalWrite(LED_BOTTOM_RIGHT,LOW);
}

byte getRandomButton() { return random(4); }

void lightUpButton(byte index) {
  resetLEDs();
  switch (index) {
    case 0: digitalWrite(LED_TOP_LEFT,    HIGH); break;
    case 1: digitalWrite(LED_TOP_RIGHT,   HIGH); break;
    case 2: digitalWrite(LED_BOTTOM_LEFT, HIGH); break;
    case 3: digitalWrite(LED_BOTTOM_RIGHT,HIGH); break;
  }
  activeButton = index;
  ledTurnedOnTime = millis();
  Firebase.RTDB.setInt(&fbdo,    "/gameStatus/currentLED", index);
  Firebase.RTDB.setString(&fbdo, "/gameStatus/status",     "playing");
}

void blinkAll(int times, int delayTime) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_TOP_LEFT,    HIGH);
    digitalWrite(LED_TOP_RIGHT,   HIGH);
    digitalWrite(LED_BOTTOM_LEFT, HIGH);
    digitalWrite(LED_BOTTOM_RIGHT,HIGH);
    delay(delayTime);
    resetLEDs();
    delay(delayTime);
  }
}

void setStartButtonBlinkMode(StartButtonBlinkMode mode) {
  startButtonBlinkMode = mode;
  lastStartButtonBlinkAt = millis();

  if (mode == START_BLINK_OFF) {
    startButtonLedState = false;
    digitalWrite(START_BUTTON_LED, LOW);
    return;
  }

  startButtonLedState = true;
  digitalWrite(START_BUTTON_LED, HIGH);
}

void updateStartButtonBlink() {
  unsigned long now = millis();

  if (startButtonBlinkMode == START_BLINK_READY) {
    // Steady ON -- corner LED chase handles the animation
    if (!startButtonLedState) {
      startButtonLedState = true;
      digitalWrite(START_BUTTON_LED, HIGH);
    }
  } else if (startButtonBlinkMode == START_BLINK_POWER_READY) {
    // Fast blink for super-power ready
    const unsigned long blinkInterval = 180;
    if (now - lastStartButtonBlinkAt >= blinkInterval) {
      lastStartButtonBlinkAt = now;
      startButtonLedState = !startButtonLedState;
      digitalWrite(START_BUTTON_LED, startButtonLedState ? HIGH : LOW);
    }
  } else {
    // OFF
    if (startButtonLedState) {
      startButtonLedState = false;
      digitalWrite(START_BUTTON_LED, LOW);
    }
  }
}

// Non-blocking clockwise chase: TL -> TR -> BR -> BL -> repeat
void updateReadyAnimation() {
  if (isGameRunning || startButtonBlinkMode != START_BLINK_READY) {
    if (!isGameRunning) {
      for (byte i = 0; i < 4; i++) {
        digitalWrite(READY_ANIM_PINS[i], LOW);
      }
    }
    return;
  }

  unsigned long now = millis();
  if (now - lastReadyAnimAt < READY_ANIM_INTERVAL) {
    return;
  }
  lastReadyAnimAt = now;

  for (byte i = 0; i < 4; i++) {
    digitalWrite(READY_ANIM_PINS[i], LOW);
  }
  digitalWrite(READY_ANIM_PINS[readyAnimStep], HIGH);

  readyAnimStep = (readyAnimStep + 1) % 4;
}

// ── Sound functions ──
void playLevelUpSound() {
  int melody[]   = { NOTE_C5, NOTE_D5, NOTE_E5, NOTE_G5 };
  int duration[] = { 120, 120, 120, 250 };
  for (int i = 0; i < 4; i++) { tone(BUZZER_PIN, melody[i], duration[i]); delay(duration[i] + 40); }
  noTone(BUZZER_PIN);
}

void playStreakSound() {
  int melody[]   = { NOTE_C5, NOTE_E5, NOTE_G5 };
  int duration[] = { 70, 70, 120 };
  for (int i = 0; i < 3; i++) { tone(BUZZER_PIN, melody[i], duration[i]); delay(duration[i] + 20); }
  noTone(BUZZER_PIN);
}

void playMissionImpossible() {
  int melody[] = {
    NOTE_G4, NOTE_G4, NOTE_AS4, NOTE_C5,
    NOTE_G4, NOTE_G4, NOTE_F4,
    NOTE_G4, NOTE_G4, NOTE_AS4, NOTE_C5,
    NOTE_G4, NOTE_G4, NOTE_F4
  };
  int duration[] = { 150,150,150,300, 150,150,400, 150,150,150,300, 150,150,400 };
  for (int i = 0; i < 14; i++) { tone(BUZZER_PIN, melody[i], duration[i]); delay(duration[i] + 40); }
  noTone(BUZZER_PIN);
}

void playGameOverMelody() {
  int melody[]   = { NOTE_E4, NOTE_D4, NOTE_C4, NOTE_D4, NOTE_C4 };
  int duration[] = { 200, 200, 400, 200, 500 };
  for (int i = 0; i < 5; i++) { tone(BUZZER_PIN, melody[i], duration[i]); delay(duration[i] + 50); }
  noTone(BUZZER_PIN);
}

// NEW: 3-2-1 countdown beeps before game starts
void playCountdownBeeps() {
  for (int i = 0; i < 3; i++) {
    tone(BUZZER_PIN, NOTE_C5, 100);
    delay(100);
    noTone(BUZZER_PIN);
    delay(400);
  }
  // GO! — higher, longer
  tone(BUZZER_PIN, NOTE_G5, 300);
  delay(320);
  noTone(BUZZER_PIN);
}

// NEW: descending wrong-press warning tone
void playWrongPressSound() {
  int melody[]   = { NOTE_G4, NOTE_E4, NOTE_C4 };
  int duration[] = { 80, 80, 180 };
  for (int i = 0; i < 3; i++) { tone(BUZZER_PIN, melody[i], duration[i]); delay(duration[i] + 20); }
  noTone(BUZZER_PIN);
}

// NEW: shield absorption rising tone
void playShieldAbsorbSound() {
  tone(BUZZER_PIN, NOTE_A4, 80);  delay(90);
  tone(BUZZER_PIN, NOTE_E5, 160); delay(170);
  noTone(BUZZER_PIN);
}

// NEW: speed boost activation jingle
void playSpeedBoostSound() {
  int melody[]   = { NOTE_C5, NOTE_E5, NOTE_G5, NOTE_C5 };
  int duration[] = { 60, 60, 60, 200 };
  for (int i = 0; i < 4; i++) { tone(BUZZER_PIN, melody[i], duration[i]); delay(duration[i] + 15); }
  noTone(BUZZER_PIN);
}

// ── Firebase helpers ──
void sendLevelEvent(int stageNumber) {
  Firebase.RTDB.setInt(&fbdo, "/gameStatus/levelEventStage", stageNumber);
  Firebase.RTDB.setInt(&fbdo, "/gameStatus/levelEvent",      millis());
}

void sendSuperPowerEvent(String title, String detail, String state) {
  Firebase.RTDB.setString(&fbdo, "/gameStatus/superPowerTitle",  title);
  Firebase.RTDB.setString(&fbdo, "/gameStatus/superPowerDetail", detail);
  Firebase.RTDB.setString(&fbdo, "/gameStatus/superPowerState",  state);
  Firebase.RTDB.setBool(&fbdo,   "/gameStatus/superPowerReady",  hasSuperPower);
  Firebase.RTDB.setInt(&fbdo,    "/gameStatus/superPowerEvent",  millis());
}

unsigned long getEffectiveElapsedTimeSeconds() {
  unsigned long elapsed = (millis() - startTime) / 1000;
  return elapsed > (unsigned long)appliedTimeCutSeconds
         ? elapsed - appliedTimeCutSeconds : 0;
}

void sendGameResult(unsigned long timeTaken, int stageReached, const String& resultStatus) {
  String playerName, playerImage;
  if (Firebase.RTDB.getString(&fbdo, "/gameControl/playerName"))  playerName  = fbdo.stringData();  else playerName  = "Unknown";
  if (Firebase.RTDB.getString(&fbdo, "/gameControl/playerImage")) playerImage = fbdo.stringData();  else playerImage = "";

  Firebase.RTDB.setInt(&fbdo,    "/gameControl/timeTaken",  timeTaken);
  Firebase.RTDB.setInt(&fbdo,    "/gameControl/stage",      stageReached);
  Firebase.RTDB.setInt(&fbdo,    "/gameControl/bestStreak", bestStreak);
  Firebase.RTDB.setString(&fbdo, "/gameControl/status",     resultStatus);

  String path = "/gameResults/" + String(millis());
  Firebase.RTDB.setString(&fbdo, path + "/name",        playerName);
  Firebase.RTDB.setString(&fbdo, path + "/playerName",  playerName);
  Firebase.RTDB.setString(&fbdo, path + "/image",       playerImage);
  Firebase.RTDB.setString(&fbdo, path + "/playerImage", playerImage);
  Firebase.RTDB.setInt(&fbdo,    path + "/timeTaken",   timeTaken);
  Firebase.RTDB.setInt(&fbdo,    path + "/stage",       stageReached);
  Firebase.RTDB.setInt(&fbdo,    path + "/bestStreak",  bestStreak);
  Firebase.RTDB.setString(&fbdo, path + "/status",      resultStatus);
  Firebase.RTDB.setInt(&fbdo,    path + "/score",       currentScore);
  Firebase.RTDB.setInt(&fbdo,    "/gameControl/score",  currentScore);
  
  int avgReactionTime = (totalHits > 0) ? (totalReactionTime / totalHits) : 0;
  Firebase.RTDB.setInt(&fbdo,    path + "/avgReactionTime", avgReactionTime);

  Firebase.RTDB.setBool(&fbdo,   path + "/usedSuperPower",
      appliedTimeCutSeconds > 0 || usedDoublePoints || usedOnePointFivePoints || usedShield || usedSpeedBoost);
  Firebase.RTDB.setInt(&fbdo,    path + "/timeCutSeconds",  appliedTimeCutSeconds);
  Firebase.RTDB.setBool(&fbdo,   path + "/usedShield",      usedShield);
  Firebase.RTDB.setBool(&fbdo,   path + "/usedSpeedBoost",  usedSpeedBoost);
  Firebase.RTDB.setBool(&fbdo,   path + "/usedDoublePoints", usedDoublePoints);
  Firebase.RTDB.setBool(&fbdo,   path + "/usedOnePointFivePoints", usedOnePointFivePoints);

  Serial.println("Game result sent!");
}

void resetSuperPowerState() {
  hasSuperPower         = false;
  currentSuperPowerType = "";
  pendingTimeCutSeconds = 0;
  appliedTimeCutSeconds = 0;
  usedDoublePoints       = false;
  usedOnePointFivePoints = false;
  shieldActive          = false;
  usedShield            = false;
  speedBoostHitsLeft    = 0;
  usedSpeedBoost        = false;
  Firebase.RTDB.setBool(&fbdo,   "/gameStatus/superPowerReady",  false);
  Firebase.RTDB.setString(&fbdo, "/gameStatus/superPowerTitle",  "");
  Firebase.RTDB.setString(&fbdo, "/gameStatus/superPowerDetail", "");
  Firebase.RTDB.setString(&fbdo, "/gameStatus/superPowerState",  "none");

  if (isGameRunning) {
    setStartButtonBlinkMode(START_BLINK_OFF);
  } else {
    setStartButtonBlinkMode(START_BLINK_READY);
  }
}

// ── 5 super powers ──
void grantSuperPower() {
  if (hasSuperPower) return;
  hasSuperPower = true;
  setStartButtonBlinkMode(START_BLINK_POWER_READY);

  int r = random(5);
  if      (r == 0) currentSuperPowerType = "timeCut";
  else if (r == 1) currentSuperPowerType = "x2Points";
  else if (r == 2) currentSuperPowerType = "x1.5Points";
  else if (r == 3) currentSuperPowerType = "perfectShield";
  else             currentSuperPowerType = "speedBoost";

  pendingTimeCutSeconds = (currentSuperPowerType == "timeCut") ? TIME_CUT_SECONDS : 0;

  if      (currentSuperPowerType == "x2Points")      sendSuperPowerEvent("Super Power!", "x2 Points",     "received");
  else if (currentSuperPowerType == "x1.5Points")    sendSuperPowerEvent("Super Power!", "x1.5 Score",    "received");
  else if (currentSuperPowerType == "perfectShield") sendSuperPowerEvent("Super Power!", "Perfect Shield","received");
  else if (currentSuperPowerType == "speedBoost")    sendSuperPowerEvent("Super Power!", "Speed Boost",   "received");
  else                                               sendSuperPowerEvent("Super Power!", "-5 Second",     "received");

  Serial.print("Super power granted: "); Serial.println(currentSuperPowerType);
}

void activateSuperPower() {
  if (!isGameRunning || !hasSuperPower) return;

  if (currentSuperPowerType == "x2Points") {
    currentScore *= 2;
    Firebase.RTDB.setInt(&fbdo, "/gameStatus/score", currentScore);
    usedDoublePoints = true;
    sendSuperPowerEvent("x2 Activated!", "Score Doubled!", "activated");

  } else if (currentSuperPowerType == "x1.5Points") {
    currentScore = (int)(currentScore * 1.5f);
    Firebase.RTDB.setInt(&fbdo, "/gameStatus/score", currentScore);
    usedOnePointFivePoints = true;
    sendSuperPowerEvent("x1.5 Activated!", "Score x1.5!", "activated");

  } else if (currentSuperPowerType == "perfectShield") {
    shieldActive = true;
    usedShield   = true;
    sendSuperPowerEvent("Shield Active!", "Next miss blocked!", "activated");

  } else if (currentSuperPowerType == "speedBoost") {
    speedBoostHitsLeft = SPEED_BOOST_HITS;
    usedSpeedBoost     = true;
    playSpeedBoostSound();
    sendSuperPowerEvent("Speed Boost!", "Double Phase x5!", "activated");

  } else {
    appliedTimeCutSeconds += pendingTimeCutSeconds;
    sendSuperPowerEvent("Time Cut!", "-5 Second applied!", "activated");
  }

  pendingTimeCutSeconds = 0;
  hasSuperPower         = false;
  currentSuperPowerType = "";
  setStartButtonBlinkMode(START_BLINK_OFF);

  // activation chime (skip for speed boost which has its own)
  if (!usedSpeedBoost || speedBoostHitsLeft == 0) {
    tone(BUZZER_PIN, NOTE_G5, 140); delay(150);
    tone(BUZZER_PIN, NOTE_C5, 120); delay(130);
    noTone(BUZZER_PIN);
  }
}

// ================= GAME CONTROL =================
void startGame() {
  if (isGameRunning) return;

  Serial.println("Game Starting — Countdown!");
  setStartButtonBlinkMode(START_BLINK_OFF);

  currentStage  = 1;
  currentPhase  = 0;
  currentStreak = 0;
  bestStreak    = 0;
  lastCorrectPressTime = 0;
  currentScore  = 0;
  totalReactionTime = 0;
  totalHits = 0;
  resetSuperPowerState();
  setStartButtonBlinkMode(START_BLINK_OFF);

  // Signal web UI to show countdown overlay
  Firebase.RTDB.setInt(&fbdo, "/gameStatus/countdownEvent", millis());
  Firebase.RTDB.setString(&fbdo, "/gameStatus/status", "countdown");

  // Hardware 3-2-1-GO beeps (~2 s)
  playCountdownBeeps();

  isGameRunning = true;
  startTime     = millis();

  Firebase.RTDB.setInt(&fbdo,    "/gameStatus/streak",       currentStreak);
  Firebase.RTDB.setInt(&fbdo,    "/gameStatus/bestStreak",   bestStreak);
  Firebase.RTDB.setInt(&fbdo,    "/gameStatus/stage",        currentStage);
  Firebase.RTDB.setInt(&fbdo,    "/gameStatus/score",        currentScore);
  Firebase.RTDB.setInt(&fbdo,    "/gameStatus/currentPhase", currentPhase);
  Firebase.RTDB.setString(&fbdo, "/gameControl/status",      "playing");
  Firebase.RTDB.setInt(&fbdo,    "/gameControl/stage",       currentStage);
  sendLevelEvent(currentStage);

  activeButton = getRandomButton();
  lightUpButton(activeButton);
}

void endGame(bool success) {
  if (!isGameRunning) return;
  isGameRunning = false;
  resetLEDs();
  setStartButtonBlinkMode(START_BLINK_READY);

  if (success) {
    Firebase.RTDB.setString(&fbdo, "/gameStatus/status", "win");
    elapsedTime = getEffectiveElapsedTimeSeconds();
    sendGameResult(elapsedTime, currentStage, "completed");
    playMissionImpossible();
    blinkAll(5, 200);
  } else {
    Firebase.RTDB.setString(&fbdo, "/gameStatus/status", "gameover");
    elapsedTime = getEffectiveElapsedTimeSeconds();
    sendGameResult(elapsedTime, currentStage, "gameover");
    playGameOverMelody();
  }

  Firebase.RTDB.setInt(&fbdo, "/gameStatus/streak",     currentStreak);
  Firebase.RTDB.setInt(&fbdo, "/gameStatus/bestStreak", bestStreak);
}

void correctButtonPressed() {
  unsigned long now = millis();
  unsigned long reactionTime = now - ledTurnedOnTime;
  totalReactionTime += reactionTime;
  totalHits++;

  if (lastCorrectPressTime > 0 && (now - lastCorrectPressTime) <= STREAK_WINDOW) {
    currentStreak++;
  } else {
    currentStreak = 1;
  }

  // Stage-scaled scoring
  int stageIdx   = constrain(currentStage - 1, 0, 2);
  int baseScore  = BASE_SCORES[stageIdx];
  int streakBonus = STREAK_BONUSES[stageIdx];
  currentScore  += baseScore;
  if (currentStreak > 1) currentScore += streakBonus;

  lastCorrectPressTime = now;
  if (currentStreak > bestStreak) bestStreak = currentStreak;

  // Speed Boost: each hit counts as 2 phases for up to SPEED_BOOST_HITS
  currentPhase++;
  if (speedBoostHitsLeft > 0) {
    currentPhase++;
    speedBoostHitsLeft--;
    Serial.print("Speed boost remaining: "); Serial.println(speedBoostHitsLeft);
  }

  Serial.printf("Stage %d | Phase %lu/%d | Streak %d | Score %d\n",
    currentStage, currentPhase, STAGE_TARGET_PHASE[currentStage - 1],
    currentStreak, currentScore);

  Firebase.RTDB.setInt(&fbdo, "/gameStatus/streak",       currentStreak);
  Firebase.RTDB.setInt(&fbdo, "/gameStatus/bestStreak",   bestStreak);
  Firebase.RTDB.setInt(&fbdo, "/gameStatus/stage",        currentStage);
  Firebase.RTDB.setInt(&fbdo, "/gameStatus/score",        currentScore);
  Firebase.RTDB.setInt(&fbdo, "/gameStatus/currentPhase", currentPhase);

  // Hit beep
  tone(BUZZER_PIN, 587, 100);
  delay(100);
  noTone(BUZZER_PIN);

  // Streak milestone every 3 hits → grant super power
  if (currentStreak > 0 && currentStreak % 3 == 0) {
    Serial.println("Hot streak!");
    blinkAll(1, 70);
    playStreakSound();
    grantSuperPower();
  }

  // Stage completion
  if (currentPhase >= STAGE_TARGET_PHASE[currentStage - 1]) {
    if (currentStage < TOTAL_STAGES) {
      currentStage++;
      currentPhase = 0;
      Serial.printf("Stage up → %d\n", currentStage);
      Firebase.RTDB.setInt(&fbdo, "/gameStatus/stage",        currentStage);
      Firebase.RTDB.setInt(&fbdo, "/gameControl/stage",       currentStage);
      Firebase.RTDB.setInt(&fbdo, "/gameStatus/currentPhase", 0);
      sendLevelEvent(currentStage);
      blinkAll(currentStage, 150);
      playLevelUpSound();
    } else {
      endGame(true);
      playMissionImpossible();
      return;
    }
  }

  activeButton = getRandomButton();
  lightUpButton(activeButton);
}

void checkButtons() {
  if (!isGameRunning) return;
  if (millis() - lastButtonPressTime < BUTTON_COOLDOWN) return;

  static bool lastTL = HIGH, lastTR = HIGH, lastBL = HIGH, lastBR = HIGH;
  bool curTL = digitalRead(BUTTON_TOP_LEFT);
  bool curTR = digitalRead(BUTTON_TOP_RIGHT);
  bool curBL = digitalRead(BUTTON_BOTTOM_LEFT);
  bool curBR = digitalRead(BUTTON_BOTTOM_RIGHT);

  int pressed = -1;
  if (lastTL == HIGH && curTL == LOW) pressed = 0;
  else if (lastTR == HIGH && curTR == LOW) pressed = 1;
  else if (lastBL == HIGH && curBL == LOW) pressed = 2;
  else if (lastBR == HIGH && curBR == LOW) pressed = 3;

  if (pressed >= 0) {
    lastButtonPressTime = millis();
    if (activeButton == pressed) {
      correctButtonPressed();
    } else {
      if (shieldActive) {
        // Shield absorbs the wrong press
        shieldActive = false;
        Serial.println("Shield absorbed wrong press!");
        playShieldAbsorbSound();
        sendSuperPowerEvent("Shield Used!", "Miss Absorbed!", "activated");
        // Advance to new LED
        activeButton = getRandomButton();
        lightUpButton(activeButton);
      } else {
        playWrongPressSound();
        endGame(false);
      }
    }
  }

  lastTL = curTL; lastTR = curTR; lastBL = curBL; lastBR = curBR;
}

// ================= LOOP =================
void loop() {
  updateStartButtonBlink();
  updateReadyAnimation();

  if (digitalRead(START_BUTTON) == LOW) {
    delay(50); // debounce
    if (digitalRead(START_BUTTON) == LOW) {
      if (!isGameRunning) {
        startGame();
      } else {
        activateSuperPower();
      }
      while (digitalRead(START_BUTTON) == LOW);
      delay(50);
    }
  }

  if (isGameRunning) {
    checkButtons();
  }
}
