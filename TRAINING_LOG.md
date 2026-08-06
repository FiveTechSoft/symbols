# Transformer CUDA Training - Development Log

## Goal
Train a Transformer model on GPU (CUDA) using the FiveTech Harbour/FWH dataset.
Pure C/CUDA implementation, no Python. Full forward + backward pass + Adam optimizer.

## Environment
- GPU: NVIDIA GeForce RTX 3060, 12GB VRAM, CUDA 12.8, Compute 8.6
- OS: Windows, Visual Studio 18, NVCC requires `-allow-unsupported-compiler`
- Dataset: `harbour_fwh_v3.jsonl` — 5,139 samples, loaded from `C:\temp\`
- Model config: DM=128, DF=256, SQ=32, LR=0.000005, BETA1=0.9, BETA2=0.999

## What Works
- Forward pass: fully GPU-accelerated (embed, attention, layernorm, FFN, softmax)
- Backward pass: layernorm, FFN, attention, embedding scatter — all correct
- Adam optimizer with NaN guard and weight decay
- Global gradient norm clipping (L2 norm, max_norm=10.0)
- Save/load model weights (binary format)
- Text generation / inference (autoregressive, full causal attention)
- Teacher-forced evaluation (check per-position accuracy)
- Training is **stable** — no NaNs, loss decreases
- Throughput: ~74 samples/s (with clipping), ~211 samples/s (without)

## Bugs Found and Fixed (7 total)

### 1. `h` buffer overflow in weight init
**Fix:** `int max_w = DM*DF; if(DM*voc>max_w) max_w=DM*voc;`

### 2. Embedding buffer overflow
**Fix:** Allocate `he` with `se = voc*DM*sizeof(float)`.

### 3. Missing `ag_df1g` zeroing between steps
**Fix:** Added `cudaMemset(m->ag_df1g,0,S*F*sizeof(float))` to zero-gradients section.

### 4. Missing residual gradient in `d_dec_e`
**Fix:** Added `k_add(ag_dde, ag_dl1, ag_dde, S*D)` after the three `k_gA` calls.

### 5. Wrong `d_dec_e` computation (doubled first term)
**Fix:** Call `k_gA` three times directly with `ag_dde` zeroed first.

### 6. Layernorm backward formula
**Correct:** `dx = s * (g*dY - dot/C - xhat*dot2/C)`

### 7. NaN detection too narrow
**Fix:** Changed `S*SQ` to `S*voc` for NaN check.

### 8. Missing residual gradient `+ dl1` in `d_dec_e`
**Fix:** `d_dec_e = dQ*Wq^T + dK*Wk^T + dV*Wv^T + dl1`

### 9. Data length bug (enc vs dec length)
**Fix:** Store both `len` (enc) and `dlen` (dec) in Smp struct.

## Architecture
```
Forward:
  enc_e = Embed(enc) + PosEnc
  dec_e = Embed(dec) + PosEnc
  Q,K,V = dec_e * Wq, dec_e * Wk, dec_e * Wv
  scores = Q*K^T / sqrt(D) + causal_mask
  probs = softmax(scores)
  attn_out = probs * V
  r1 = dec_e + attn_out * Wo        ← residual 1
  ln1 = LayerNorm(r1)
  f1 = gelu(ln1 * W1)
  f2 = f1 * W2
  r2 = ln1 + f2                     ← residual 2
  ln2 = LayerNorm(r2)
  logits = ln2 * Wout
  probs = softmax(logits)
  loss = CrossEntropy(probs, target)
```

## Training Results

### Full dataset (5,139 samples, 10 epochs)
| Config | Loss | Speed | Notes |
|--------|------|-------|-------|
| LR=0.00001, no clip | 3.17 | 211 s/s | Best result, but unstable across runs |
| LR=0.00001, clip=50 | 9.99 | 74 s/s | Clipping too aggressive |
| LR=0.000005, clip=10 | 4.65 | 74 s/s | Stable but plateau |
| LR=0.000005, no clip | diverges | - | Unstable without clipping |

### Generation Test Results (teacher-forced, LR=0.000005, clip=10)
| Sample | Correct | Rate | Notes |
|--------|---------|------|-------|
| #0 | 1/10 | 10% | Only predicted `n` (newline) |
| #100 | 2/32 | 6% | Only predicted `n` |
| #500 | 3/32 | 9% | Only predicted `n` at positions 4,13,22 |
| #1000 | 0/5 | 0% | Nothing correct |
| #2000 | 3/32 | 9% | Only predicted `n` at positions 4,13,22 |

### Key Finding: Model learns statistical frequency, NOT input→output mapping
- Position 0 always predicts `.` (21%)
- Position 1 always predicts `#` (90%)
- Positions 2+ always predict `n` (61%)
- Same prediction regardless of context = attention NOT learning

### Root Cause: Vocabulary too large for dataset size
- 4,096 vocab tokens / 5,139 samples = ~1.25 occurrences per token
- Model can't learn rare tokens → collapses to predicting most frequent ones
- Loss of 4.65 = perplexity ~105 (narrows to ~105 tokens but picks wrong ones)

## What Needs to Be Done to Improve

### Option 1: More data (most important)
- Need ~50,000+ samples for 4K vocab to work
- Each token needs ~10+ occurrences to learn patterns

### Option 2: Smaller vocabulary
- Reduce to 512 or 1,024 tokens
- Each token appears 5-10x more often
- Simple: count token frequency, keep top N

### Option 3: Larger model
- Current: DM=128, DF=256 (1.18M params)
- Better: DM=256, DF=512 (5.2M params)
- More capacity = can memorize more patterns

### Option 4: Combination (recommended)
- Vocab 1,024 + DM=256 + DF=512 + more data

## Files
- `train_cuda_full.cu` — main CUDA training file (all-in-one)
- `train_cuda.cu` — earlier forward-only version
- `test_cuda.cu` — GPU test (261 GFLOPS verified)
- `compile_cuda.bat` — batch file for nvcc compilation
- `harbour_fwh_v3.jsonl` — dataset (5,139 samples)
- `TRAINING_LOG.md` — this file
