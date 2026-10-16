"""구조판 AI 서버 예시 (파이썬 표준 라이브러리만 사용).

시뮬레이션 페이지(simulation/evac-compare.html)는 http://localhost:8000 에 이 서버가 떠 있으면
화살표 방향 판단을 이 서버에 맡기고, 없으면 페이지 안의 내장 신경망을 씁니다.

실행:  python ai/server_example.py

우리 AI 모델로 바꾸려면 predict_right() 안만 고치면 됩니다.
입출력 형식은 ai/README.md 참고.
"""

import json
from http.server import BaseHTTPRequestHandler, HTTPServer

PORT = 8000
NAME = "예시 규칙 기반 서버"   # 페이지 AI 패널에 표시되는 이름. 모델 이름으로 바꾸세요.


def predict_right(features):
    """오른쪽 계단이 더 안전할 확률(0~1)을 돌려준다.

    features (6개, 페이지가 보냄):
      0: 왼쪽 계단까지 거리 (0~1)      1: 오른쪽 계단까지 거리 (0~1)
      2: 왼쪽 복도 경로 연기량 (0~4)   3: 오른쪽 복도 경로 연기량 (0~4)
      4: 왼쪽 계단 연기량 (0~4)        5: 오른쪽 계단 연기량 (0~4)
    """
    dist_l, dist_r, corr_l, corr_r, stair_l, stair_r = features
    cost_l = dist_l * 20 + 60 * (corr_l * 5 + stair_l * 3.5)
    cost_r = dist_r * 20 + 60 * (corr_r * 5 + stair_r * 3.5)
    diff = (cost_l - cost_r) / 20
    return 1 / (1 + 2.718281828 ** (-diff))


class Handler(BaseHTTPRequestHandler):
    def _send(self, code, body):
        data = json.dumps(body, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_OPTIONS(self):
        self._send(204, {})

    def do_GET(self):
        if self.path.startswith("/health"):
            self._send(200, {"ok": True, "name": NAME})
        else:
            self._send(404, {"error": "not found"})

    def do_POST(self):
        if not self.path.startswith("/predict"):
            self._send(404, {"error": "not found"})
            return
        try:
            length = int(self.headers.get("Content-Length", 0))
            req = json.loads(self.rfile.read(length) or b"{}")
            probs = [float(predict_right(item["features"])) for item in req.get("items", [])]
            self._send(200, {"p_right": probs})
        except Exception as err:  # 잘못된 요청이면 페이지는 내장 신경망으로 돌아간다
            self._send(400, {"error": str(err)})

    def log_message(self, *args):
        pass


if __name__ == "__main__":
    print(f"구조판 AI 서버 실행 중: http://localhost:{PORT}  ({NAME})")
    HTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
