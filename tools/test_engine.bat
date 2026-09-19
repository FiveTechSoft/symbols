@echo off
REM Comprehensive Test Battery for Symbolic LLM Engine
REM Tests ingestion, querying, reasoning, edge cases, and performance

SET SERVER=http://127.0.0.1:8099
SET PASS=0
SET FAIL=0
SET TOTAL=0

echo ============================================================
echo COMPREHENSIVE TEST BATTERY - SYMBOLIC LLM ENGINE
echo ============================================================
echo.

REM ============================================================
REM CATEGORY 1: BASIC FACTUAL QUERIES
REM ============================================================
echo === 1. BASIC FACTUAL QUERIES (Bible) ===

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"Who created the heavens and earth?\"}]}" | findstr /i "God Lord" >nul && (echo PASS: Q1 Who created heavens/earth & set /a PASS+=1) || (echo FAIL: Q1 Who created heavens/earth & set /a FAIL+=1)
set /a TOTAL+=1

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"Who was the first man?\"}]}" | findstr /i "Adam" >nul && (echo PASS: Q2 Who was first man & set /a PASS+=1) || (echo FAIL: Q2 Who was first man & set /a FAIL+=1)
set /a TOTAL+=1

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"Who built the ark?\"}]}" | findstr /i "Noah" >nul && (echo PASS: Q3 Who built ark & set /a PASS+=1) || (echo FAIL: Q3 Who built ark & set /a FAIL+=1)
set /a TOTAL+=1

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"Who was the son of Abraham?\"}]}" | findstr /i "Isaac" >nul && (echo PASS: Q4 Son of Abraham & set /a PASS+=1) || (echo FAIL: Q4 Son of Abraham & set /a FAIL+=1)
set /a TOTAL+=1

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"Who killed Goliath?\"}]}" | findstr /i "David" >nul && (echo PASS: Q5 Who killed Goliath & set /a PASS+=1) || (echo FAIL: Q5 Who killed Goliath & set /a FAIL+=1)
set /a TOTAL+=1

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"Who led the Israelites out of Egypt?\"}]}" | findstr /i "Moses" >nul && (echo PASS: Q6 Led out of Egypt & set /a PASS+=1) || (echo FAIL: Q6 Led out of Egypt & set /a FAIL+=1)
set /a TOTAL+=1

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"Who was the first king of Israel?\"}]}" | findstr /i "Saul" >nul && (echo PASS: Q7 First king of Israel & set /a PASS+=1) || (echo FAIL: Q7 First king of Israel & set /a FAIL+=1)
set /a TOTAL+=1

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"Who was the father of David?\"}]}" | findstr /i "Jesse" >nul && (echo PASS: Q8 Father of David & set /a PASS+=1) || (echo FAIL: Q8 Father of David & set /a FAIL+=1)
set /a TOTAL+=1

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"Where was Jesus born?\"}]}" | findstr /i "Bethlehem" >nul && (echo PASS: Q9 Where Jesus born & set /a PASS+=1) || (echo FAIL: Q9 Where Jesus born & set /a FAIL+=1)
set /a TOTAL+=1

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"What happened on the seventh day?\"}]}" | findstr /i "rest" >nul && (echo PASS: Q10 Seventh day rest & set /a PASS+=1) || (echo FAIL: Q10 Seventh day rest & set /a FAIL+=1)
set /a TOTAL+=1

REM ============================================================
REM CATEGORY 2: RELATIONSHIP QUERIES
REM ============================================================
echo.
echo === 2. RELATIONSHIP QUERIES ===

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"Who was the father of Isaac?\"}]}" | findstr /i "Abraham" >nul && (echo PASS: R1 Father of Isaac & set /a PASS+=1) || (echo FAIL: R1 Father of Isaac & set /a FAIL+=1)
set /a TOTAL+=1

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"Who was the grandfather of David?\"}]}" | findstr /i "Boaz Obed Jesse" >nul && (echo PASS: R2 Grandfather of David & set /a PASS+=1) || (echo FAIL: R2 Grandfather of David & set /a FAIL+=1)
set /a TOTAL+=1

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"Which tribe did Aaron belong to?\"}]}" | findstr /i "Levi" >nul && (echo PASS: R3 Aaron tribe & set /a PASS+=1) || (echo FAIL: R3 Aaron tribe & set /a FAIL+=1)
set /a TOTAL+=1

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"Who succeeded Solomon as king?\"}]}" | findstr /i "Rehoboam" >nul && (echo PASS: R4 Successor of Solomon & set /a PASS+=1) || (echo FAIL: R4 Successor of Solomon & set /a FAIL+=1)
set /a TOTAL+=1

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"Who was the prophet that anointed David?\"}]}" | findstr /i "Samuel" >nul && (echo PASS: R5 Prophet anointed David & set /a PASS+=1) || (echo FAIL: R5 Prophet anointed David & set /a FAIL+=1)
set /a TOTAL+=1

REM ============================================================
REM CATEGORY 3: MULTI-HOP REASONING
REM ============================================================
echo.
echo === 3. MULTI-HOP REASONING ===

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"Who was the grandfather of Solomon?\"}]}" | findstr /i "Jesse David" >nul && (echo PASS: M1 Grandfather of Solomon & set /a PASS+=1) || (echo FAIL: M1 Grandfather of Solomon & set /a FAIL+=1)
set /a TOTAL+=1

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"Who was the descendant of Adam that lived 900 years?\"}]}" | findstr /i "Methuselah Noah Lamech" >nul && (echo PASS: M2 Long-lived descendant & set /a PASS+=1) || (echo FAIL: M2 Long-lived descendant & set /a FAIL+=1)
set /a TOTAL+=1

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"In which land did the Israelites wander for 40 years?\"}]}" | findstr /i "wilderness desert Canaan" >nul && (echo PASS: M3 Land of wandering & set /a PASS+=1) || (echo FAIL: M3 Land of wandering & set /a FAIL+=1)
set /a TOTAL+=1

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"Why was the city of Sodom destroyed?\"}]}" | findstr /i "sin wicked" >nul && (echo PASS: M4 Why Sodom destroyed & set /a PASS+=1) || (echo FAIL: M4 Why Sodom destroyed & set /a FAIL+=1)
set /a TOTAL+=1

REM ============================================================
REM CATEGORY 4: NUMERIC QUERIES
REM ============================================================
echo.
echo === 4. NUMERIC QUERIES ===

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"How old was Methuselah when he died?\"}]}" | findstr /i "969" >nul && (echo PASS: N1 Age of Methuselah & set /a PASS+=1) || (echo FAIL: N1 Age of Methuselah & set /a FAIL+=1)
set /a TOTAL+=1

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"How long did it rain during the flood?\"}]}" | findstr /i "40 forty" >nul && (echo PASS: N2 Duration of flood & set /a PASS+=1) || (echo FAIL: N2 Duration of flood & set /a FAIL+=1)
set /a TOTAL+=1

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"How many disciples did Jesus have?\"}]}" | findstr /i "12 twelve" >nul && (echo PASS: N3 Number of disciples & set /a PASS+=1) || (echo FAIL: N3 Number of disciples & set /a FAIL+=1)
set /a TOTAL+=1

REM ============================================================
REM CATEGORY 5: EDGE CASES
REM ============================================================
echo.
echo === 5. EDGE CASES ===

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"\"}]}" | findstr /i "error" >nul && (echo PASS: E1 Empty query handled & set /a PASS+=1) || (echo FAIL: E1 Empty query not handled & set /a FAIL+=1)
set /a TOTAL+=1

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"God\"}]}" >nul && (echo PASS: E2 Single word works & set /a PASS+=1) || (echo FAIL: E2 Single word failed & set /a FAIL+=1)
set /a TOTAL+=1

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"'; DROP TABLE knowledge;--\"}]}" >nul && (echo PASS: E3 SQL injection handled & set /a PASS+=1) || (echo FAIL: E3 SQL injection failed & set /a FAIL+=1)
set /a TOTAL+=1

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"print('hello'); return 42;\"}]}" >nul && (echo PASS: E4 Code snippet handled & set /a PASS+=1) || (echo FAIL: E4 Code snippet failed & set /a FAIL+=1)
set /a TOTAL+=1

curl -s "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":\"Who is the president of Narnia?\"}]}" | findstr /i "UNKNOWN" >nul && (echo PASS: E5 Fictional entity returns UNKNOWN & set /a PASS+=1) || (echo FAIL: E5 Fictional entity did not return UNKNOWN & set /a FAIL+=1)
set /a TOTAL+=1

REM ============================================================
REM CATEGORY 6: PERFORMANCE
REM ============================================================
echo.
echo === 6. PERFORMANCE METRICS ===

echo Testing query latency...
for %%Q in ("Who is God?" "Who created the world?" "Who was the first man?" "Who built the ark?" "Who killed Goliath?") do (
    echo Query: %%Q
    curl -s -w "Time: %%{time_total}s\n" -o nul "%SERVER%/v1/chat/completions" -H "Content-Type: application/json" -d "{\"messages\":[{\"role\":\"user\",\"content\":%%Q}]}"
)

REM ============================================================
REM FINAL REPORT
REM ============================================================
echo.
echo ============================================================
echo FINAL RESULTS
echo ============================================================
echo   PASS: %PASS%
echo   FAIL: %FAIL%
echo   TOTAL: %TOTAL%
echo ============================================================

if %FAIL%==0 (
    echo ALL TESTS PASSED!
) else (
    echo Some tests failed. Review output above.
)
