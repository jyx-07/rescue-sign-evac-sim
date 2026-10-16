@echo off
chcp 65001 >nul
rem 더블클릭하면 시뮬레이션을 http://localhost:8765 로 띄우고 크롬(없으면 엣지)으로 엽니다.
rem 파이썬이 설치되어 있어야 합니다.
cd /d "%~dp0"
set URL=http://localhost:8765/evac-compare.html
start "sim-server" /min python -m http.server 8765
timeout /t 2 >nul
reg query "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\chrome.exe" >nul 2>nul && (start "" chrome "%URL%") || (start "" msedge "%URL%")
echo 시뮬레이션 실행 중: %URL%
echo 최소화된 sim-server 창을 닫으면 서버가 꺼집니다.
timeout /t 5 >nul
