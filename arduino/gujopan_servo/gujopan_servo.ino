// 구조판 표지판 서보 제어 (아두이노 우노)
//
// 시뮬레이션 페이지(evac-compare.html)가 USB 시리얼로 보내는 명령을 받아
// 층마다 2개, 모두 4개의 서보로 표지판 화살표를 돌린다.
//
// 명령 형식: 한 줄에 하나, "S,<서보번호>,<L|R>"   예) S,2,R
//   서보 0 = 1층 왼쪽 표지판    서보 1 = 1층 오른쪽 표지판
//   서보 2 = 2층 왼쪽 표지판    서보 3 = 2층 오른쪽 표지판
//
// 배선: 서보 신호선을 아래 SERVO_PINS에 연결한다.
//   서보 4개를 USB 5V로 돌리면 전류가 모자라 보드가 재부팅될 수 있다.
//   서보 전원(빨강)은 외부 5V 전원에 연결하고, 외부 전원 GND와 아두이노 GND를 꼭 이어 준다.

#include <Servo.h>

const int SERVO_COUNT = 4;
const int SERVO_PINS[SERVO_COUNT] = {3, 5, 6, 9};

// 화살표가 왼쪽/오른쪽을 가리킬 때의 각도. 표지판을 단 방향에 맞춰 서보마다 조정한다.
const int ANGLE_LEFT[SERVO_COUNT]  = {20, 20, 20, 20};
const int ANGLE_RIGHT[SERVO_COUNT] = {160, 160, 160, 160};

// 시작할 때 방향: 일반 표지판처럼 가까운 계단 (왼쪽 표지판은 왼쪽, 오른쪽 표지판은 오른쪽)
const char START_DIR[SERVO_COUNT] = {'L', 'R', 'L', 'R'};

// 회전 속도: 이 간격(ms)마다 1도씩 움직인다. 클수록 천천히 돈다.
const unsigned long STEP_MS = 6;

Servo servos[SERVO_COUNT];
int current[SERVO_COUNT];
int target[SERVO_COUNT];
unsigned long lastStep = 0;

char line[24];
int lineLen = 0;

int angleFor(int i, char dir) {
  return dir == 'R' ? ANGLE_RIGHT[i] : ANGLE_LEFT[i];
}

void handleLine(const char *cmd) {
  // "S,<i>,<L|R>"
  if (cmd[0] != 'S' || cmd[1] != ',') return;
  int i = cmd[2] - '0';
  char dir = cmd[4];
  if (i < 0 || i >= SERVO_COUNT || cmd[3] != ',' || (dir != 'L' && dir != 'R')) {
    Serial.print("ERR ");
    Serial.println(cmd);
    return;
  }
  target[i] = angleFor(i, dir);
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
