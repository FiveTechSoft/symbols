@echo off
rem BookBrain chat — doble clic o: chat.bat
rem Consola en UTF-8: el chat emite tildes (Si, Preguntame...).
rem KB curado (alice_clean): 45 hechos verificados. El bulk
rem (alice.knowledge.pl, puerta 22%) sigue en cuarentena.
chcp 65001 >nul
cd /d "%~dp0"
"C:\Program Files\swipl\bin\swipl.exe" -s chat.pl -g "chat('alice_clean.knowledge.pl')" -t halt
