# Master Algorithm Unification Map (2015–2026)

**Compiled:** 2026-09-13  
**Scope:** Real attempts to unify (or approach) Pedro Domingos’s five ML “tribes” from *The Master Algorithm* (2015) under one learner / training procedure.  
**Method:** Live web + arXiv metadata fetches; citations below were verified via arXiv abs pages, Nature, ACM/IEEE DOIs, or primary PDFs. No fabricated papers.

## Domingos’s five tribes (reference)

| # | Tribe | Core idea | Canonical methods |
|---|--------|-----------|-------------------|
| 1 | Symbolists | Inverse deduction / logic | Decision trees, ILP, logic programs |
| 2 | Connectionists | Credit assignment in nets | Backprop, deep learning |
| 3 | Evolutionaries | Fitness + variation | Genetic programming, ES |
| 4 | Bayesians | Probabilistic inference | Graphical models, MCMC, VI |
| 5 | Analogizers | Similarity / kernels | k-NN, SVMs, kernel machines |

**What would count as a true Master Algorithm here:** one training procedure (not a multi-module product of separately trained systems) that structurally exercises all five inductive biases, with empirical evidence beyond a single tribe’s benchmark suite.

---

## Critiques & framing (read first)

These constrain how seriously to take any “unification” claim.

1. **Ernest Davis — review of Domingos (2016-ish)**  
   - *Machine Learning and the Prospect of a Master Algorithm*  
   - URL: https://cs.nyu.edu/~davise/papers/OldReviews/MasterAlgorithm.pdf  
   - Argument: the Master Algorithm thesis is under-specified and overclaims what one learner can derive from data; the book’s survey of tribes is useful, the universality claim is not.

2. **Rich Sutton — *The Bitter Lesson* (2019)**  
   - URL: http://www.incompleteideas.net/IncIdeas/BitterLesson.html  
   - Argument: general methods that scale with compute (search + learning) beat hand-encoded tribal knowledge. Reads as skepticism toward elegant multi-tribe hybrids that don’t scale, and as support for “one scalable procedure” (today: foundation-model pretraining / ICL).

3. **No Free Lunch (Wolpert & Macready, 1997)**  
   - *No Free Lunch Theorems for Optimization*, IEEE Trans. Evolutionary Computation.  
   - DOI: https://doi.org/10.1109/4235.585893  
   - Implication: without assumptions on the problem distribution, no learner dominates all others. A Master Algorithm must smuggle inductive bias (or a meta-distribution over tasks).

**Takeaway:** Post-2015 “unifiers” are best scored as (a) how many tribes they *structurally* cover, (b) whether training is *one* procedure, (c) what scaled empirically—not as solved AGI blueprints.

---

## Candidate dossiers

### 1. GFlowNets (Generative Flow Networks)

| Field | Detail |
|-------|--------|
| **Name** | GFlowNets / MLE-GFN |
| **Key papers** | Bengio, Jain, Korablyov, Precup, Bengio — *Flow Network based Generative Models for Non-Iterative Diverse Candidate Generation* (2021), arXiv:[2106.04399](https://arxiv.org/abs/2106.04399) · Bengio, Lahlou, Deleu, Hu, Tiwari, Bengio — *GFlowNet Foundations* (2021; JMLR lineage), arXiv:[2111.09266](https://arxiv.org/abs/2111.09266) · Whitammer, Jain, Bengio, Sun, Bengio — *Trajectory balance: Improved credit assignment in GFlowNets* (2022), arXiv:[2201.13259](https://arxiv.org/abs/2201.13259) *(arXiv author list as of 2026; earlier literature cites Nikolay Malkin as lead; code gist historically under malkin1729)* · Zhang, Chen, Malkin, Bengio — *Unifying Generative Models with GFlowNets and Beyond* (2022), arXiv:[2209.02606](https://arxiv.org/abs/2209.02606) |
| **Year** | 2021–2023 (active through 2026) |
| **Tribes covered** | **Connectionists** (neural forward policy); **Bayesians** (sample ∝ reward; amortizes MCMC; partition functions); **Evolutionaries** (diversity / multi-modal sampling, Pareto variants in Foundations); **Symbolists** *(partial)* when objects are graphs/programs/sets; **Analogizers** *(weak / not structural)* |
| **Empirically shown** | Diverse molecule / discrete object generation; improved credit assignment with trajectory balance; MLE-GFN improves discrete & continuous generative baselines vs standard training; theory for flows, detailed balance, marginals |
| **Still missing** | Native kernel/similarity tribe; classical ILP/logic induction; a single recipe that is also competitive as a pure deep supervised learner *and* evolutionary searcher at foundation-model scale |
| **Verdict** | **Closest explicit theoretical unifier of generative + inferential tribes — real Master-Algorithm *candidate* for Conn+Bayes(+Evo diversity), still a strong hybrid short of all five.** |

---

### 2. Foundation models + Algorithm Distillation / in-context meta-learning

| Field | Detail |
|-------|--------|
| **Name** | Algorithm Distillation (AD); transformers as in-context algorithm emulators; TAIL |
| **Key papers** | Laskin et al. — *In-context Reinforcement Learning with Algorithm Distillation* (2022), arXiv:[2210.14215](https://arxiv.org/abs/2210.14215) · Li, Jiao, Huang, Wei, Chen — *Transformers Meet In-Context Learning: A Universal Approximation Theory* (2025), arXiv:[2506.05200](https://arxiv.org/abs/2506.05200) · Woerner, Oh, Baumgartner — *Universal Algorithm-Implicit Learning* (TAIL) (2026), arXiv:[2602.14761](https://arxiv.org/abs/2602.14761) |
| **Year** | 2022–2026 |
| **Tribes covered** | **Connectionists** (the substrate); **Analogizers** *(emergent)* via ICL ≈ locally weighted / nearest-neighbor-in-context behavior; **Bayesians / Symbolists / Evolutionaries** only insofar as those *algorithms* can be distilled or prompted into context — coverage is **meta**, not structural |
| **Empirically shown** | AD: causal transformer trained on RL learning histories improves policy *in-context* without weight updates, sometimes more data-efficient than the source algorithm (toy/mid RL envs). TAIL (2026): non-causal transformer meta-learner with cross-modal / variable label-space transfer claims. Universal-approximation-style ICL theory (2025). |
| **Still missing** | Guarantees that one pretraining run internalizes *all* tribal algorithms; symbolic soundness; evolutionary open-endedness; calibrated Bayesian posteriors without extra machinery |
| **Verdict** | **Bitter-Lesson-style Master-Algorithm *candidate* (one loss, one architecture, many algorithms in context) — empirically a meta-hybrid, not a five-tribe theory.** Highly relevant to 1B LMs. |

---

### 3. DreamCoder (wake-sleep Bayesian program learning)

| Field | Detail |
|-------|--------|
| **Name** | DreamCoder |
| **Key papers** | Ellis, Wong, Nye, Sablé-Meyer, Cary, Morales, Hewitt, Solar-Lezama, Tenenbaum — *DreamCoder: Growing generalizable, interpretable knowledge with wake-sleep Bayesian program learning* (2020), arXiv:[2006.08381](https://arxiv.org/abs/2006.08381) · PLDI’21 version: *DreamCoder: Bootstrapping Inductive Program Synthesis with Wake-Sleep Library Learning*, DOI:[10.1145/3453483.3454080](https://doi.org/10.1145/3453483.3454080) |
| **Year** | 2020–2021 |
| **Tribes covered** | **Symbolists** (program induction / library learning ≈ modern ILP); **Bayesians** (wake-sleep Bayesian program learning); **Connectionists** (neural recognition / search guide); weak Evo / Analogizer |
| **Empirically shown** | Solves classic inductive programming + creative drawing/building tasks from few examples; rediscovers functional programming idioms, vector algebra, Newton/Coulomb-style laws in equation domains; library+neural policy bootstrap |
| **Still missing** | Scale to foundation-model data/compute; evolutionary open search; kernel methods; robust perception at ImageNet/LLM scale without a separate net |
| **Verdict** | **Strongest clean 3-tribe (Symb+Bayes+Conn) unification with one wake-sleep procedure — elite hybrid, not full Master Algorithm.** |

---

### 4. JEPA / energy-based world models (LeCun AMI agenda)

| Field | Detail |
|-------|--------|
| **Name** | JEPA / H-JEPA / V-JEPA; latent-variable EBMs |
| **Key papers** | LeCun — *A Path Towards Autonomous Machine Intelligence* (2022), OpenReview:[BZ5a1r-kVsf](https://openreview.net/forum?id=BZ5a1r-kVsf) · Assran et al. — *Self-Supervised Learning from Images with a Joint-Embedding Predictive Architecture* (I-JEPA) (2023), arXiv:[2301.08243](https://arxiv.org/abs/2301.08243) · Dawid & LeCun — *Introduction to Latent Variable Energy-Based Models…* (2023), arXiv:[2306.02572](https://arxiv.org/abs/2306.02572) · Bardes et al. — *Revisiting Feature Prediction for Learning Visual Representations from Video* (V-JEPA) (2024), arXiv:[2404.08471](https://arxiv.org/abs/2404.08471) · Historical EBM frame: LeCun et al., *A Tutorial on Energy-Based Learning* (2006), http://yann.lecun.com/exdb/publis/pdf/lecun-06.pdf |
| **Year** | Agenda 2022; empirical I/V-JEPA 2023–2024; continuing 2025–2026 |
| **Tribes covered** | **Connectionists**; **Bayesians** *(partial via energy ≈ unnormalized models)*; planning-as-inference flavor; **Symbolists / Evolutionaries / Analogizers** not first-class (kernels historically related to energies, but not SVM-style in JEPA) |
| **Empirically shown** | I-JEPA: strong ImageNet linear probes without handcrafted augmentations; V-JEPA: competitive video representation learning by predicting in feature space (Kinetics / SSv2 numbers reported in paper); EBM tutorial lineage unifies many losses as energy + loss |
| **Still missing** | Demonstrated symbolic reasoning; evolutionary diversity search; a single training run that also matches LLM-style language AGI claims; robust long-horizon planning at scale still largely aspirational in the 2022 position paper |
| **Verdict** | **Ambitious world-model Master-Algorithm *agenda*; empirically a Conn(+energy) hybrid with planning aspirations — not yet five-tribe.** |

---

### 5. Neuro-symbolic probabilistic programming (DeepProbLog & kin) + AlphaGeometry

| Field | Detail |
|-------|--------|
| **Name** | DeepProbLog; Logic Tensor Networks; AlphaGeometry; NeSy surveys |
| **Key papers** | Manhaeve, Dumančić, Kimmig, Demeester, De Raedt — *DeepProbLog: Neural Probabilistic Logic Programming* (2018), arXiv:[1805.10872](https://arxiv.org/abs/1805.10872) · De Raedt, Dumančić, Manhaeve, Marra — *From Statistical Relational to Neuro-Symbolic Artificial Intelligence* (2020), arXiv:[2003.08316](https://arxiv.org/abs/2003.08316) · Badreddine, d’Avila Garcez, Serafini, Spranger — *Logic Tensor Networks* (2020), arXiv:[2012.13635](https://arxiv.org/abs/2012.13635) · Trinh et al. — *Solving olympiad geometry without human demonstrations* (AlphaGeometry), *Nature* (2024), DOI:[10.1038/s41586-023-06747-5](https://doi.org/10.1038/s41586-023-06747-5) |
| **Year** | 2018–2024 |
| **Tribes covered** | **Symbolists** + **Connectionists** + **Bayesians** (DeepProbLog neural predicates + ProbLog); AlphaGeometry is **Conn LM + symbolic deduction engine** (product system) |
| **Empirically shown** | DeepProbLog: end-to-end learning on perception+reasoning toys (e.g., MNIST addition). LTN: differentiable fuzzy FOL for classification/clustering/query tasks. AlphaGeometry: 25/30 IMO-geometry problems, near gold-medalist average, trained on synthetic data |
| **Still missing** | Single homogeneous training procedure (AlphaGeometry is explicitly hybrid modules); Evolutionaries; Analogizers; scaling DeepProbLog-style exact inference |
| **Verdict** | **DeepProbLog: strong 3-tribe hybrid with one gradient story. AlphaGeometry: spectacular hybrid product, not one Master training algorithm.** |

---

### 6. Bayesian deep learning / variational nets

| Field | Detail |
|-------|--------|
| **Name** | Bayesian Deep Learning (BDL); BNNs; VI + SG-MCMC |
| **Key papers** | Wang & Yeung — *A Survey on Bayesian Deep Learning* (arXiv 2016; ACM CSUR 2020/21), arXiv:[1604.01662](https://arxiv.org/abs/1604.01662), DOI:[10.1145/3409383](https://doi.org/10.1145/3409383) · Chen, Li, Zhang, Li — *Bayesian Computation in Deep Learning* (2025), arXiv:[2502.18300](https://arxiv.org/abs/2502.18300) |
| **Year** | 2015–2026 (ongoing) |
| **Tribes covered** | **Bayesians** + **Connectionists** by construction; others not |
| **Empirically shown** | Uncertainty-aware nets, deep generative models (VAEs etc.), recommender/topic/control applications in surveys; scalable VI and SG-MCMC tooling for large nets |
| **Still missing** | Symbolists, Evolutionaries, Analogizers as native citizens; often approximate posteriors poorly calibrated at LLM scale |
| **Verdict** | **Mature 2-tribe unification framework — important substrate, not a Master Algorithm.** |

---

### 7. Evolutionary ↔ gradient / QD hybrids + Evolution through Large Models

| Field | Detail |
|-------|--------|
| **Name** | ME-ES; PGA-MAP-Elites; QD-PG; ELM / OpenELM |
| **Key papers** | Colas, Huizinga, Madhavan, Clune — *Scaling MAP-Elites to Deep Neuroevolution* (ME-ES) (2020), arXiv:[2003.01825](https://arxiv.org/abs/2003.01825) · Nilsson & Cully — *Policy Gradient Assisted MAP-Elites* (GECCO 2021), DOI:[10.1145/3449639.3459304](https://doi.org/10.1145/3449639.3459304) · Pierrot et al. — *Diversity Policy Gradient for Sample Efficient Quality-Diversity Optimization* (2020), arXiv:[2006.08505](https://arxiv.org/abs/2006.08505) · Lehman, Gordon, Jain, Ndousse, Yeh, Stanley — *Evolution through Large Models* (2022), arXiv:[2206.08896](https://arxiv.org/abs/2206.08896) · OpenELM library (CarperAI): https://github.com/CarperAI/OpenELM |
| **Year** | 2020–2023 |
| **Tribes covered** | **Evolutionaries** + **Connectionists**; ELM uses LLMs as mutation operators (Conn substrate for Evo search over programs) |
| **Empirically shown** | ME-ES / PGA-MAP-Elites / QD-PG: QD archives of deep neural controllers with better sample efficiency via ES or policy gradients; ELM: LLM-driven MAP-Elites evolves diverse Python programs (Sodarace etc.) |
| **Still missing** | Bayesian calibration, symbolic soundness guarantees, analogizer kernels; not one loss that also does supervised LLM pretraining |
| **Verdict** | **Strong Evo+Conn hybrids (ELM especially relevant to code LMs) — not a five-tribe Master Algorithm.** |

---

### 8. Flow Matching (generative unification — narrow)

| Field | Detail |
|-------|--------|
| **Name** | Flow Matching |
| **Key papers** | Lipman, Chen, Ben-Hamu, Nickel, Le — *Flow Matching for Generative Modeling* (2022; ICLR 2023), arXiv:[2210.02747](https://arxiv.org/abs/2210.02747) |
| **Year** | 2022–2023 |
| **Tribes covered** | Mostly **Connectionists** (+ continuous normalizing flows / diffusion-path **Bayesian generative** view) |
| **Empirically shown** | Simulation-free training of CNFs; OT paths competitive with diffusion training |
| **Still missing** | Almost all tribal breadth Domingos cares about |
| **Verdict** | **Important unifier *within* deep generative modeling — not a Master-Algorithm candidate for the five tribes.** Listed to avoid conflating “unifying generative models” with Domingos unification. |

---

## Ranking: closest to covering **all five** tribes with **one training procedure**

Scoring heuristic (0–5): +1 per tribe structurally engaged; −0.5 if coverage is only “can emulate via prompting/modules”; −0.5 if multiple separately trained systems required.

| Rank | Candidate | Approx. tribe score | One procedure? | Notes |
|------|-----------|---------------------|----------------|-------|
| 1 | **GFlowNets** | Conn+Bayes+Evo(diversity)+Symb(partial) ≈ 3.5–4 | Yes (flow / TB / DB objectives) | Best *explicit* multi-tribe learning theory post-2015 |
| 2 | **AD / ICL / foundation-model meta-learning (incl. TAIL)** | Conn + Analog(emergent) + meta-others ≈ 3–3.5 (soft) | Yes (LM loss / AD) | Best *scalable* “one learner many algorithms”; tribe coverage soft |
| 3 | **DreamCoder** | Symb+Bayes+Conn = 3 | Yes (wake-sleep) | Cleanest classical three-tribe story |
| 4 | **JEPA / EBM world models** | Conn+Bayes(energy) ≈ 2–2.5 | Yes (predictive embedding / energy) | Strong agenda; limited tribal breadth so far |
| 5 | **DeepProbLog / NeSy PP** | Symb+Bayes+Conn = 3 | Mostly yes (grads through logic) | Inference cost / scale limits |
| 6 | **AlphaGeometry-style NeSy products** | Symb+Conn = 2 | **No** (LM + symbolic engine) | SOTA results ≠ Master Algorithm |
| 7 | **BDL / VI+nets** | Bayes+Conn = 2 | Yes | Substrate, not unifier of five |
| 8 | **QD–gradient / ELM** | Evo+Conn = 2 | Hybrid loop | Essential evolutionary chapter |

**Honest bottom line (2026):** Nobody has a demonstrated single training procedure that fully owns all five Domingos tribes. The live frontier is (i) **GFlowNets** as probabilistic-generative unifier, (ii) **foundation-model ICL / algorithm distillation** as compute-scaling unifier, (iii) **DreamCoder-like** neurosymbolic Bayesian program learning as the best “old tribes” synthesis—plus NeSy systems that win benchmarks as *assemblies*.

---

## Falsifiable hypotheses (laptop / ≤1B scale)

Designed to be runnable on a workstation with a small model (TinyLlama-class or smaller). Not “train GPT-5.”

1. **H1 — Tiny GFlowNet vs MCMC diversity.** On a compositional discrete task (e.g., bitstrings or small graphs with multimodal reward), a GFlowNet with trajectory-balance loss (≤5M params) matches or exceeds Metropolis–Hastings in *unique high-reward modes recovered* at a fixed sample budget (e.g., 10k samples). **Falsify if:** MH recovers strictly more modes at equal wall-clock on CPU/GPU.

2. **H2 — Algorithm Distillation at toy RL scale.** A causal transformer (≤50M) trained on Q-learning or REINFORCE learning histories for a family of gridworlds / bandits shows *in-context* return improvement across episodes **without weight updates** on held-out task IDs. **Falsify if:** return does not rise within context beyond a frozen behavioral-cloning baseline trained on final expert actions only.

3. **H3 — Constrained / neuro-symbolic decoding on TinyLlama-1B.** For exact multi-digit arithmetic or simple definite-clause reasoning, grammar-constrained or logic-checked decoding (or a tiny DeepProbLog-style predicate head) raises exact-match accuracy by ≥10 absolute points vs unconstrained greedy decoding at same model+prompt. **Falsify if:** no significant gain after controlling for prompt length and rejection-sampling budget.

4. **H4 — Mini DreamCoder library learning.** On a list-processing DSL with ≤20 tasks, wake-sleep library learning with a small neural guide rediscovers at least one non-trivial abstraction (e.g., `map`/`fold`) within a fixed iteration/compute budget on one GPU. **Falsify if:** neural guide never beats enumerative baseline on tasks solved / time, or no abstraction enters the library.

5. **H5 — JEPA vs reconstruction for tiny world models.** On MiniGrid or a small video toy, latent prediction (JEPA-style) yields longer reliable multi-step latent rollout (lower feature prediction error at horizon H≥8) than pixel reconstruction under identical encoder size and train steps. **Falsify if:** reconstruction matches or beats JEPA on latent probe + rollout metrics.

---

## TinyLlama / 1B-class relevance

The user also works on improving TinyLlama-scale models. Highest-leverage links:

| Direction | Why it fits 1B | Concrete hook |
|-----------|----------------|---------------|
| **Algorithm Distillation / ICL** | Meta-learning lives in context length, not param count alone | Distill small RL or optimizer histories into TinyLlama; measure in-context improvement |
| **Neuro-symbolic constraints** | 1B models are weak at exact reasoning; symbols externalize precision | Constrained decoding, tool/solver calls, DeepProbLog-like predicate heads on embeddings |
| **GFlowNet fine-tuning** | Diversity of solutions matters for synthetic data / search | Fine-tune policy head to sample diverse valid chains (math, code) ∝ reward |
| **Bayesian / ensemble PEFT** | Uncertainty on small models is actionable | LoRA ensembles or last-layer VI for selective prediction / active learning |
| **ELM-style evolution of adapters/prompts** | Cheap outer loop around a frozen 1B | MAP-Elites over prompt programs or LoRA seeds for quality-diversity of behaviors |
| **JEPA-style objectives** | Less about LLM text; useful if TinyLlama work expands to multimodal | Predictive embedding losses for any non-text tower |

**Practical priority for TinyLlama work:** H2 + H3 first (ICL distillation + symbolic constraints), then GFlowNet diverse decoding for data generation.

---

## Sources checklist (verified)

| ID / DOI | Title (short) | Accessed via |
|----------|---------------|--------------|
| arXiv:2106.04399 | Flow Network GFlowNet intro | abs metadata |
| arXiv:2111.09266 | GFlowNet Foundations | abs + WebSearch |
| arXiv:2201.13259 | Trajectory balance | abs + PDF p.1 |
| arXiv:2209.02606 | Unifying Generative Models with GFlowNets | abs |
| arXiv:2006.08381 | DreamCoder | abs |
| arXiv:2210.14215 | Algorithm Distillation | abs |
| arXiv:2506.05200 | Transformers Meet ICL (theory) | abs |
| arXiv:2602.14761 | Universal Algorithm-Implicit Learning | abs |
| arXiv:2301.08243 | I-JEPA | abs |
| arXiv:2404.08471 | V-JEPA | abs |
| arXiv:2306.02572 | Latent Variable EBMs (Dawid & LeCun) | abs |
| OpenReview BZ5a1r-kVsf | Path Towards AMI (LeCun) | OpenReview link |
| arXiv:1805.10872 | DeepProbLog | abs |
| arXiv:2003.08316 | Statistical Relational → NeSy | abs |
| arXiv:2012.13635 | Logic Tensor Networks | abs |
| DOI:10.1038/s41586-023-06747-5 | AlphaGeometry (Nature 2024) | Nature page |
| arXiv:1604.01662 | Survey on Bayesian Deep Learning | abs |
| arXiv:2502.18300 | Bayesian Computation in Deep Learning | abs |
| arXiv:2003.01825 | Scaling MAP-Elites (ME-ES) | abs |
| DOI:10.1145/3449639.3459304 | PGA-MAP-Elites | ACM |
| arXiv:2006.08505 | Diversity Policy Gradient (QD-PG) | abs |
| arXiv:2206.08896 | Evolution through Large Models | abs |
| arXiv:2210.02747 | Flow Matching | abs |
| incompleteideas.net | The Bitter Lesson | primary essay |
| cs.nyu.edu/~davise/.../MasterAlgorithm.pdf | Davis critique | primary PDF |
| DOI:10.1109/4235.585893 | NFL theorems | IEEE |

---

*End of map. Companion machine-readable ranking: `candidates.json`.*
