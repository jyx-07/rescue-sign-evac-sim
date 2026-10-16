// 구조판 표지판 서보 + 초음파 센서 + 외벽 LED + 가스 센서 (아두이노 우노)
//
// 시뮬레이션 페이지(evac-compare.html)와 USB 시리얼(115200bps)로 주고받는다.
//
// 페이지 → 아두이노 (한 줄에 하나)
//   "S,<서보번호>,<L|R>"  표지판 화살표 방향.   예) S,2,R
//     서보 0 = 1층 왼쪽 표지판    서보 1 = 1층 오른쪽 표지판
//     서보 2 = 2층 왼쪽 표지판    서보 3 = 2층 오른쪽 표지판
//   "L,<LED번호>,<0|1>"  외벽 LED 끄기/켜기.   예) L,0,1
//     LED 0 = 202호 외벽    LED 1 = 204호 외벽
//
// 아두이노 → 페이지
//   "U,<센서번호>,<0|1>"  초음파 센서가 사람을 감지하면 1, 사라지면 0.
//     센서 0 = 202호    센서 1 = 204호
//   페이지는 이 값을 받아 그 방에 사람이 있다고 보고, 사다리차를 그 방으로 보낸다.
//   "G,<값>"  가스 센서 값(0~1023). 화면에 표시만 한다.
//   "FIRE"    가스 값이 기준을 넘으면 한 번 보낸다. 페이지는 이 신호를 받으면 화재를 시작한다.
//   (팀에서 만든 가스 센서 코드를 따로 쓴다면, 화재로 판단할 때 Serial.println("FIRE"); 한 줄만 보내면 된다.)
//
// 배선
//   서보 신호선: 3, 5, 6, 9번 핀
//   초음파 센서(HC-SR04) 0: TRIG 7, ECHO 8     센서 1: TRIG 10, ECHO 11   (VCC 5V, GND)
//   외벽 LED 0: A0, LED 1: A1   (각각 220Ω 저항을 거쳐 GND로)
//   가스 센서(MQ-2 등) 아날로그 출력: A2   (VCC 5V, GND)
//   서보 4개를 USB 5V로 돌리면 전류가 모자라 보드가 재부팅될 수 있다.
//   서보 전원(빨강)은 외부 5V에 연결하고, 외부 전원 GND와 아두이노 GND를 꼭 이어 준다.

#include <Servo.h>

// ---------- 서보 ----------
const int SERVO_COUNT = 4;
const int SERVO_PINS[SERVO_COUNT] = {3, 5, 6, 9};
// 화살표가 왼쪽/오른쪽을 가리킬 때의 각도. 표지판을 단 방향에 맞춰 서보마다 조정한다.
const int ANGLE_LEFT[SERVO_COUNT]  = {20, 20, 20, 20};
const int ANGLE_RIGHT[SERVO_COUNT] = {160, 160, 160, 160};
// 시작할 때 방향: 일반 표지판처럼 가까운 계단 (왼쪽 표지판은 왼쪽, 오른쪽 표지판은 오른쪽)
const char START_DIR[SERVO_COUNT] = {'L', 'R', 'L', 'R'};
// 회전 속도: 이 간격(ms)마다 1도씩 움직인다. 클수록 천천히 돈다.
const unsigned long STEP_MS = 6;

// ---------- 초음파 센서 + 외벽 LED ----------
const int SONAR_COUNT = 2;
const int TRIG_PINS[SONAR_COUNT] = {7, 10};
const int ECHO_PINS[SONAR_COUNT] = {8, 11};
const int LED_PINS[SONAR_COUNT]  = {A0, A1};
// 이 거리(cm)보다 가까우면 사람이 있다고 본다. 꺼질 때는 조금 더 멀어져야 해서 깜빡임을 막는다.
const int PRESENT_CM = 30;
const int ABSENT_CM  = 40;
// 같은 판정이 이 횟수만큼 연속으로 나와야 상태를 바꾼다
const int CONFIRM = 3;
const unsigned long SONAR_MS = 60;   // 센서 하나를 읽는 간격

// ---------- 가스 센서 ----------
const int GAS_PIN = A2;
const int GAS_THRESHOLD = 400;          // 이 값 이상이면 화재로 본다. 센서와 환경에 맞춰 조정한다.
const unsigned long GAS_WARMUP_MS = 20000;  // MQ 센서는 켜고 나서 한동안 값이 높게 나오므로 이 시간 동안은 화재 판단을 하지 않는다
const unsigned long GAS_MS = 300;       // 가스 값을 보내는 간격
const unsigned long FIRE_REARM_MS = 15000;  // 값이 기준 아래로 내려가고 이 시간이 지나면 다시 FIRE를 보낼 수 있다

Servo servos[SERVO_COUNT];
int current[SERVO_COUNT];
int target[SERVO_COUNT];
unsigned long lastStep = 0;

bool present[SONAR_COUNT];
int streak[SONAR_COUNT];
int sonarTurn = 0;
unsigned long lastSonar = 0;

unsigned long lastGas = 0;
bool fireSent = false;
unsigned long belowSince = 0;

char line[24];
int lineLen = 0;

int angleFor(int i, char dir) {
  return dir == 'R' ? ANGLE_RIGHT[i] : ANGLE_LEFT[i];
}

// 거리(cm). 응답이 없으면 -1
long readDistanceCm(int i) {
  digitalWrite(TRIG_PINS[i], LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PINS[i], HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PINS[i], LOW);
  unsigned long us = pulseIn(ECHO_PINS[i], HIGH, 25000UL);
  if (us == 0) return -1;
  return us / 58;
}

void updateSonar(int i) {
  long cm = readDistanceCm(i);
  bool near = cm > 0 && cm < PRESENT_CM;
  bool far = cm < 0 || cm > ABSENT_CM;
  bool flip = present[i] ? far : near;
  streak[i] = flip ? streak[i] + 1 : 0;
  if (streak[i] >= CONFIRM) {
    present[i] = !present[i];
    streak[i] = 0;
    Serial.print("U,");
    Serial.print(i);
    Serial.print(',');
    Serial.println(present[i] ? 1 : 0);
  }
}

void handleLine(const char *cmd) {
  // "S,<i>,<L|R>" 또는 "L,<i>,<0|1>"
  if (cmd[1] != ',' || cmd[3] != ',') return;
  int i = cmd[2] - '0';
  char v = cmd[4];
  if (cmd[0] == 'S' && i >= 0 && i < SERVO_COUNT && (v == 'L' || v == 'R')) {
    target[i] = angleFor(i, v);
  } else if (cmd[0] == 'L' && i >= 0 && i < SONAR_COUNT && (v == '0' || v == '1')) {
    digitalWrite(LED_PINS[i], v == '1' ? HIGH : LOW);
  } else {
    Serial.print("ERR ");
    Serial.println(cmd);
    return;
  }
  Serial.print("OK ");
  Serial.println(cmd);
}

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < SERVO_COUNT; i++) {
    current[i] = target[i] = angleFor(i, START_DIR[i]);
    servos[i].attach(SERVO_PINS[i]);
    servos[i].write(current[i]);
  }
  for (int i = 0; i < SONAR_COUNT; i++) {
    pinMode(TRIG_PINS[i], OUTPUT);
    pinMode(ECHO_PINS[i], INPUT);
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
    present[i] = false;
    streak[i] = 0;
  }
  Serial.println("READY");
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (lineLen > 0) {
        line[lineLen] = '\0';
        handleLine(line);
        lineLen = 0;
      }
    } else if (lineLen < (int)sizeof(line) - 1) {
      line[lineLen++] = c;
    }
  }

  // 가스 센서: 값은 계속 보내고, 기준을 넘으면 FIRE를 한 번 보낸다
  if (millis() - lastGas >= GAS_MS) {
    lastGas = millis();
    int gas = analogRead(GAS_PIN);
    Serial.print("G,");
    Serial.println(gas);
    if (millis() > GAS_WARMUP_MS) {
      if (gas >= GAS_THRESHOLD) {
        belowSince = 0;
        if (!fireSent) { Serial.println("FIRE"); fireSent = true; }
      } else if (fireSent) {
        if (belowSince == 0) belowSince = millis();
        if (millis() - belowSince > FIRE_REARM_MS) fireSent = false;
      }
    }
  }

  // 초음파 센서는 번갈아 하나씩 읽는다 (동시에 쏘면 서로 간섭)
  if (millis() - lastSonar >= SONAR_MS) {
    lastSonar = millis();
    updateSonar(sonarTurn);
    sonarTurn = (sonarTurn + 1) % SONAR_COUNT;
  }

  // 목표 각도까지 조금씩 움직여 화살표가 천천히 회전하게 한다
  if (millis() - lastStep >= STEP_MS) {
    lastStep = millis();
    for (int i = 0; i < SERVO_COUNT; i++) {
      if (current[i] != target[i]) {
        current[i] += current[i] < target[i] ? 1 : -1;
        servos[i].write(current[i]);
      }
    }
  }
}
