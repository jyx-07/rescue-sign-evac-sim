@echo off
chcp 65001 >nul
rem 더블클릭하면 시뮬레이션을 http://localhost:8765 로 띄우고 크롬(없으면 엣지)으로 엽니다.
rem 파이썬이 설치되어 있어야 합니다.
cd /d "%~dp0"
set URL=http://localhost:8765/simulation/evac-compare.html
start "sim-server" /min python -m http.server 8765
rem 우리 AI 서버(ai\server.py)가 있으면 같이 띄운다. 없으면 페이지의 내장 신경망을 쓴다.
if exist ai\server.py start "ai-server" /min python ai\server.py
timeout /t 2 >nul
reg query "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\chrome.exe" >nul 2>nul && (start "" chrome "%URL%") || (start "" msedge "%URL%")
echo 시뮬레이션 실행 중: %URL%
echo 최소화된 sim-server 창을 닫으면 서버가 꺼집니다.
timeout /t 5 >nul
