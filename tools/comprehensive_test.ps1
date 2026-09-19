# Comprehensive Test Battery for Symbolic LLM Engine
# Tests ingestion, querying, reasoning, edge cases, and performance

$SERVER = "http://127.0.0.1:8099"
$RESULTS = @()
$PASS = 0
$FAIL = 0
$WARN = 0

function Test-Query {
    param(
        [string]$Name,
        [string]$Query,
        [string]$ExpectedContains = "",
        [string]$ExpectedNotContains = "",
        [int]$TimeoutMs = 5000
    )
    
    $body = @{
        messages = @(@{role="user"; content=$Query})
        max_tokens = 200
    } | ConvertTo-Json -Depth 3
    
    try {
        $response = Invoke-RestMethod -Uri "$SERVER/v1/chat/completions" -Method Post -Body $body -ContentType "application/json" -TimeoutSec 10
        $answer = $response.choices[0].message.content
        
        $passed = $true
        $reason = "OK"
        
        if ($ExpectedContains -and -not ($answer -match $ExpectedContains)) {
            $passed = $false
            $reason = "Expected to contain: $ExpectedContains"
        }
        if ($ExpectedNotContains -and ($answer -match $ExpectedNotContains)) {
            $passed = $false
            $reason = "Should NOT contain: $ExpectedNotContains"
        }
        
        if ($passed) {
            $script:PASS++
            Write-Host "  PASS: $Name" -ForegroundColor Green
            return @{Name=$Name; Status="PASS"; Answer=$answer; Reason=$reason}
        } else {
            $script:FAIL++
            Write-Host "  FAIL: $Name - $reason" -ForegroundColor Red
            Write-Host "    Answer: $answer" -ForegroundColor Yellow
            return @{Name=$Name; Status="FAIL"; Answer=$answer; Reason=$reason}
        }
    }
    catch {
        $script:FAIL++
        Write-Host "  FAIL: $Name - Exception: $_" -ForegroundColor Red
        return @{Name=$Name; Status="FAIL"; Answer=""; Reason="Exception: $_"}
    }
}

function Test-Batch {
    param(
        [string]$Category,
        [array]$Tests
    )
    
    Write-Host "`n=== $Category ===" -ForegroundColor Cyan
    foreach ($t in $Tests) {
        $result = Test-Query @t
        $script:RESULTS += $result
    }
}

# ============================================================
# CATEGORY 1: BASIC FACTUAL QUERIES
# ============================================================
$basicTests = @(
    @{Name="Q1: Who created the heavens and earth?"; Query="Who created the heavens and earth?"; ExpectedContains="God|Lord|creator"},
    @{Name="Q2: Who was the first man?"; Query="Who was the first man?"; ExpectedContains="Adam"},
    @{Name="Q3: Who built the ark?"; Query="Who built the ark?"; ExpectedContains="Noah"},
    @{Name="Q4: Who was Abraham's son?"; Query="Who was the son of Abraham?"; ExpectedContains="Isaac"},
    @{Name="Q5: Who killed Goliath?"; Query="Who killed Goliath?"; ExpectedContains="David"},
    @{Name="Q6: Who led the Israelites out of Egypt?"; Query="Who led the Israelites out of Egypt?"; ExpectedContains="Moses"},
    @{Name="Q7: Who was the first king of Israel?"; Query="Who was the first king of Israel?"; ExpectedContains="Saul"},
    @{Name="Q8: Who was David's father?"; Query="Who was the father of David?"; ExpectedContains="Jesse"},
    @{Name="Q9: Where was Jesus born?"; Query="Where was Jesus born?"; ExpectedContains="Bethlehem"},
    @{Name="Q10: What happened on the seventh day?"; Query="What happened on the seventh day of creation?"; ExpectedContains="rest|God rested"}
)
Test-Batch -Category "1. BASIC FACTUAL QUERIES (Bible)" -Tests $basicTests

# ============================================================
# CATEGORY 2: RELATIONSHIP QUERIES
# ============================================================
$relationTests = @(
    @{Name="R1: Father-Son chain"; Query="Who was the father of Isaac?"; ExpectedContains="Abraham"},
    @{Name="R2: Grandfather"; Query="Who was the grandfather of David?"; ExpectedContains="Boaz|Obed|Jesse"},
    @{Name="R3: Tribe affiliation"; Query="Which tribe did Aaron belong to?"; ExpectedContains="Levi|Levite"},
    @{Name="R4: Kingdom succession"; Query="Who succeeded Solomon as king?"; ExpectedContains="Rehoboam"},
    @{Name="R5: Prophet identification"; Query="Who was the prophet that anointed David?"; ExpectedContains="Samuel"},
    @{Name="R6: Enemy relationships"; Query="Who was the enemy of the Philistines?"; ExpectedContains="Israel|Israelites|David"},
    @{Name="R7: Geographic locations"; Query="Where did Moses receive the commandments?"; ExpectedContains="Sinai|mount"},
    @{Name="R8: Event sequence"; Query="What happened after the flood?"; ExpectedContains="rain|dove|ark|rainbow"}
)
Test-Batch -Category "2. RELATIONSHIP QUERIES" -Tests $relationTests

# ============================================================
# CATEGORY 3: MULTI-HOP REASONING
# ============================================================
$multiHopTests = @(
    @{Name="M1: Two-hop (David->Jesse->?)"; Query="Who was the grandfather of Solomon?"; ExpectedContains="Jesse|David"},
    @{Name="M2: Chain (Adam->Seth->Noah)"; Query="Who was the descendant of Adam that lived 900 years?"; ExpectedContains="Methuselah|Noah|Lamech"},
    @{Name="M3: Geographic reasoning"; Query="In which land did the Israelites wander for 40 years?"; ExpectedContains="wilderness|desert|Canaan"},
    @{Name="M4: Cause-effect"; Query="Why was the city of Sodom destroyed?"; ExpectedContains="sin|wicked|Lot|fire|brimstone"},
    @{Name="M5: Multiple entities"; Query="Who were the twelve sons of Jacob?"; ExpectedContains="Reuben|Simeon|Levi|Judah|Dan|Naphtali|Gad|Asher|Issachar|Zebulun|Joseph|Benjamin"}
)
Test-Batch -Category "3. MULTI-HOP REASONING" -Tests $multiHopTests

# ============================================================
# CATEGORY 4: NUMERIC QUERIES
# ============================================================
$numericTests = @(
    @{Name="N1: Age of Methuselah"; Query="How old was Methuselah when he died?"; ExpectedContains="969|969 years"},
    @{Name="N2: Duration of flood"; Query="How long did it rain during the flood?"; ExpectedContains="40|40 days|forty"},
    @{Name="N3: Number of disciples"; Query="How many disciples did Jesus have?"; ExpectedContains="12|twelve"},
    @{Name="N4: Years of wandering"; Query="How many years did the Israelites wander?"; ExpectedContains="40|forty"},
    @{Name="N5: Population count"; Query="How many Israelites left Egypt?"; ExpectedContains="600,000|600000|multitude|number"}
)
Test-Batch -Category "4. NUMERIC QUERIES" -Tests $numericTests

# ============================================================
# CATEGORY 5: NEGATION & UNKNOWN HANDLING
# ============================================================
$negationTests = @(
    @{Name="Neg1: Negative fact"; Query="Did Adam live forever?"; ExpectedContains="no|not|died|death"},
    @{Name="Neg2: Non-existent entity"; Query="Who is the president of Narnia?"; ExpectedNotContains="I don't know|UNKNOWN"},
    @{Name="Neg3: Impossible question"; Query="What color is the number 7?"; ExpectedNotContains="I don't know|UNKNOWN"},
    @{Name="Neg4: Fictional character"; Query="Is Harry Potter in the Bible?"; ExpectedNotContains="I don't know|UNKNOWN"},
    @{Name="Neg5: Tricky negation"; Query="Was there no flood?"; ExpectedContains="flood|rain"}
)
Test-Batch -Category "5. NEGATION & UNKNOWN HANDLING" -Tests $negationTests

# ============================================================
# CATEGORY 6: SEMANTIC / FUZZY MATCHING
# ============================================================
$semanticTests = @(
    @{Name="S1: Synonym (creator=God)"; Query="Who is the creator of the world?"; ExpectedContains="God|Lord"},
    @{Name="S2: Paraphrase (liberated=led out)"; Query="Who liberated the Hebrews from slavery?"; ExpectedContains="Moses|Egypt"},
    @{Name="S3: Different wording"; Query="What's the story about a big boat?"; ExpectedContains="Noah|ark|flood"},
    @{Name="S4: Conceptual"; Query="Who is the father of all nations?"; ExpectedContains="Abraham"},
    @{Name="S5: Metaphorical"; Query="Who was the serpent in the garden?"; ExpectedContains="serpent|devil|Satan|tempted"}
)
Test-Batch -Category "6. SEMANTIC / FUZZY MATCHING" -Tests $semanticTests

# ============================================================
# CATEGORY 7: PROVENCE & CITATION
# ============================================================
$provenanceTests = @(
    @{Name="P1: Source citation"; Query="Tell me about David and Goliath with the source"},
    @{Name="P2: Verse reference"; Query="Quote John 3:16"},
    @{Name="P3: Context request"; Query="What is the context of the Ten Commandments?"},
    @{Name="P4: Multiple sources"; Query="Where else is Noah mentioned?"},
    @{Name="P5: Verification"; Query="Is it true that Moses parted the Red Sea?"}
)
Test-Batch -Category "7. PROVENANCE & CITATION" -Tests $provenanceTests

# ============================================================
# CATEGORY 8: EDGE CASES & STRESS
# ============================================================
$edgeTests = @(
    @{Name="E1: Empty query"; Query=""},
    @{Name="E2: Single word"; Query="God"},
    @{Name="E3: Very long query"; Query="Who was the person who was born in Bethlehem who was the son of Jesse who was the son of Obed who was the son of Boaz who was the son of Salmon who was the son of Nahshon who was the son of Amminadab?"},
    @{Name="E4: Special characters"; Query="What about @#$%^&*()?"},
    @{Name="E5: SQL injection attempt"; Query="'; DROP TABLE knowledge;--"},
    @{Name="E6: Very short"; Query="Why?"},
    @{Name="E7: Repetitive"; Query="God God God God God?"},
    @{Name="E8: Foreign characters"; Query="Quién es Dios?"},
    @{Name="E9: All caps"; Query="WHO IS GOD"},
    @{Name="E10: Mixed case"; Query="wHo WaS aDaM?"},
    @{Name="E11: Unicode symbols"; Query="Is there a ☀️ in the Bible?"},
    @{Name="E12: Code snippet attempt"; Query="print('hello'); return 42;"}
)
Test-Batch -Category "8. EDGE CASES & STRESS" -Tests $edgeTests

# ============================================================
# CATEGORY 9: PERFORMANCE METRICS
# ============================================================
Write-Host "`n=== 9. PERFORMANCE METRICS ===" -ForegroundColor Cyan

$perfQueries = @(
    "Who is God?",
    "Who created the world?",
    "Who was the first man?",
    "Who built the ark?",
    "Who killed Goliath?"
)

$totalTime = 0
$count = 0
foreach ($q in $perfQueries) {
    $body = @{
        messages = @(@{role="user"; content=$q})
        max_tokens = 100
    } | ConvertTo-Json -Depth 3
    
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    try {
        $null = Invoke-RestMethod -Uri "$SERVER/v1/chat/completions" -Method Post -Body $body -ContentType "application/json" -TimeoutSec 10
    } catch {}
    $sw.Stop()
    
    $elapsed = $sw.ElapsedMilliseconds
    $totalTime += $elapsed
    $count++
    Write-Host "  Query: '$q' -> ${elapsed}ms" -ForegroundColor Gray
}

$avgTime = if ($count -gt 0) { $totalTime / $count } else { 0 }
Write-Host "`n  Average latency: ${avgTime}ms" -ForegroundColor Yellow
Write-Host "  Total queries: $count" -ForegroundColor Yellow

# ============================================================
# CATEGORY 10: SERVER API COMPATIBILITY
# ============================================================
Write-Host "`n=== 10. SERVER API COMPATIBILITY ===" -ForegroundColor Cyan

# Test 1: Models endpoint
try {
    $models = Invoke-RestMethod -Uri "$SERVER/v1/models" -Method Get
    if ($models.data[0].id -eq "symbols") {
        Write-Host "  PASS: /v1/models returns correct format" -ForegroundColor Green
        $PASS++
    } else {
        Write-Host "  FAIL: /v1/models format incorrect" -ForegroundColor Red
        $FAIL++
    }
} catch {
    Write-Host "  FAIL: /v1/models endpoint error" -ForegroundColor Red
    $FAIL++
}

# Test 2: Chat completion format
try {
    $chatBody = @{
        model = "symbols"
        messages = @(@{role="user"; content="Hello"})
        max_tokens = 50
    } | ConvertTo-Json -Depth 3
    
    $chatResp = Invoke-RestMethod -Uri "$SERVER/v1/chat/completions" -Method Post -Body $chatBody -ContentType "application/json"
    
    if ($chatResp.choices[0].message.content) {
        Write-Host "  PASS: Chat completion response format" -ForegroundColor Green
        $PASS++
    } else {
        Write-Host "  FAIL: Chat completion missing content" -ForegroundColor Red
        $FAIL++
    }
} catch {
    Write-Host "  FAIL: Chat completion endpoint error" -ForegroundColor Red
    $FAIL++
}

# Test 3: Session handling (send two related queries)
try {
    $q1 = @{role="user"; content="Who was David?"} | ConvertTo-Json
    $q2 = @{role="user"; content="Who was his father?"} | ConvertTo-Json
    
    $body1 = @{messages = @($($q1 | ConvertFrom-Json))} | ConvertTo-Json -Depth 3
    $resp1 = Invoke-RestMethod -Uri "$SERVER/v1/chat/completions" -Method Post -Body $body1 -ContentType "application/json"
    
    $body2 = @{messages = @($($q1 | ConvertFrom-Json), @{role="assistant"; content=$resp1.choices[0].message.content}, $($q2 | ConvertFrom-Json))} | ConvertTo-Json -Depth 3
    $resp2 = Invoke-RestMethod -Uri "$SERVER/v1/chat/completions" -Method Post -Body $body2 -ContentType "application/json"
    
    if ($resp2.choices[0].message.content) {
        Write-Host "  PASS: Multi-turn conversation works" -ForegroundColor Green
        $PASS++
    } else {
        Write-Host "  FAIL: Multi-turn conversation failed" -ForegroundColor Red
        $FAIL++
    }
} catch {
    Write-Host "  FAIL: Session handling error" -ForegroundColor Red
    $FAIL++
}

# ============================================================
# FINAL REPORT
# ============================================================
Write-Host "`n" + ("=" * 60) -ForegroundColor White
Write-Host "FINAL RESULTS" -ForegroundColor Cyan
Write-Host ("=" * 60) -ForegroundColor White
Write-Host "  PASS: $PASS" -ForegroundColor Green
Write-Host "  FAIL: $FAIL" -ForegroundColor Red
Write-Host "  TOTAL: $($PASS + $FAIL)" -ForegroundColor Yellow
Write-Host ("=" * 60) -ForegroundColor White

if ($FAIL -eq 0) {
    Write-Host "`nALL TESTS PASSED!" -ForegroundColor Green
} else {
    Write-Host "`nSome tests failed. Review output above." -ForegroundColor Yellow
}

# Return results as JSON for further processing
$RESULTS | ConvertTo-Json -Depth 3 | Out-File "C:\symbols\tools\test_results.json"
Write-Host "`nResults saved to C:\symbols\tools\test_results.json" -ForegroundColor Gray
