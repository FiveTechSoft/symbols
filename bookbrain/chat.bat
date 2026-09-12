@echo off
rem BookBrain chat — doble clic o: chat.bat
cd /d "%~dp0"
"C:\Program Files\swipl\bin\swipl.exe" -s chat.pl -g "chat('alice.knowledge.pl')" -t halt
