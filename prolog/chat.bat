@echo off
rem BookBrain chat — doble clic o: chat.bat
rem Consola en UTF-8: el chat emite tildes (Si, Preguntame...).
chcp 65001 >nul
cd /d "%~dp0"
"C:\Program Files\swipl\bin\swipl.exe" -s chat.pl -g "chat('alice.knowledge.pl')" -t halt
