#!/bin/bash
# 더블클릭하면 시뮬레이션을 http://localhost:8765 로 띄우고 크롬(없으면 엣지)으로 엽니다.
# 로컬 주소로 열어야 아두이노 허용 기록이 남아서, 다음부터 페이지를 켜자마자 자동 연결됩니다.
cd "$(dirname "$0")"
PORT=8765
URL="http://localhost:$PORT/evac-compare.html"

python3 -m http.server "$PORT" >/dev/null 2>&1 &
SERVER=$!
sleep 1

if [ -d "/Applications/Google Chrome.app" ]; then
  open -a "Google Chrome" "$URL"
elif [ -d "/Applications/Microsoft Edge.app" ]; then
  open -a "Microsoft Edge" "$URL"
else
  echo "크롬이나 엣지가 없습니다. 아두이노 연결은 크롬·엣지에서만 됩니다."
  open "$URL"
fi

echo "시뮬레이션 실행 중: $URL"
echo "이 창을 닫으면 시뮬레이션 서버도 꺼집니다."
wait "$SERVER"
