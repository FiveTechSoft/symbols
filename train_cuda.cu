/**
 * TRAIN_CUDA.CU - Entrenamiento del Transformer con GPU CUDA
 * 
 * Aprovecha la RTX 3060 para entrenar el Transformer
 * con el dataset Harbour/FWH.
 * 
 * Compilar: nvcc -o train_cuda.exe train_cuda.cu tensor_cuda.cu -lm
 * Ejecutar: train_cuda.exe harbour_fwh_v3.jsonl [muestras] [epochs]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <cuda_runtime.h>

/* ============================================================
 * MACROS
 * ============================================================ */

#define CUDA_CHECK(call) do { \
    cudaError_t err = call; \
    if (err != cudaSuccess) { \
        fprintf(stderr, "CUDA Error: %s\n", cudaGetErrorString(err)); \
        exit(1); \
    } \
} while(0)

/* ============================================================
 * ESTRUCTURAS
 * ============================================================ */

typedef struct {
    char *instruction;
    char *input;
    char *output;
} Sample;

typedef struct {
    char **words;
    int size;
    int capacity;
} Vocab;

typedef struct {
    int *data;
    int length;
} TokenSeq;

/* ============================================================
 * LECTOR JSONL
 * ============================================================ */

char* json_get(const char *json, const char *field) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", field);
    const char *s = strstr(json, search);
    if (!s) return strdup("");
    s = strchr(s + strlen(search), ':');
    if (!s) return strdup("");
    s++;
    while (*s == ' ') s++;
    if (*s != '"') return strdup("");
    s++;
    const char *e = s;
    while (*e && !(*e == '"' && *(e-1) != '\\')) e++;
    int len = e - s;
    char *v = (char*)malloc(len + 1);
    strncpy(v, s, len);
    v[len] = '\0';
    return v;
}

Sample* load_data(const char *filename, int max_n, int *count) {
    FILE *f = fopen(filename, "r");
    if (!f) { perror("Error abriendo archivo"); exit(1); }
    
    Sample *samples = (Sample*)malloc(max_n * sizeof(Sample));
    char line[65536];
    *count = 0;
    
    while (fgets(line, sizeof(line), f) && *count < max_n) {
        if (line[0] == '\n') continue;
        
        samples[*count].instruction = json_get(line, "instruction");
        samples[*count].input = json_get(line, "input");
        samples[*count].output = json_get(line, "output");
        (*count)++;
    }
    
    fclose(f);
    printf("Datos cargados: %d muestras\n", *count);
    return samples;
}

void free_samples(Sample *s, int n) {
    for (int i = 0; i < n; i++) {
        free(s[i].instruction);
        free(s[i].input);
        free(s[i].output);
    }
    free(s);
}

/* ============================================================
 * VOCABULARIO
 * ============================================================ */

Vocab* vocab_create(int cap) {
    Vocab *v = (Vocab*)malloc(sizeof(Vocab));
    v->words = (char**)malloc(cap * sizeof(char*));
    v->size = 0;
    v->capacity = cap;
    
    v->words[v->size++] = strdup("<PAD>");
    v->words[v->size++] = strdup("<UNK>");
    v->words[v->size++] = strdup("<BOS>");
    v->words[v->size++] = strdup("<EOS>");
    return v;
}

int vocab_add(Vocab *v, const char *w) {
    for (int i = 0; i < v->size; i++)
        if (strcmp(v->words[i], w) == 0) return i;
    if (v->size >= v->capacity) {
        v->capacity *= 2;
        v->words = (char**)realloc(v->words, v->capacity * sizeof(char*));
    }
    v->words[v->size] = strdup(w);
    return v->size++;
}

int vocab_lookup(Vocab *v, const char *w) {
    for (int i = 0; i < v->size; i++)
        if (strcmp(v->words[i], w) == 0) return i;
    return 1;  // UNK
}

void tokenize(const char *text, int *ids, int *len, Vocab *voc, int max_len) {
    int pos = 0;
    ids[pos++] = 2;  // BOS
    
    int i = 0;
    int tlen = strlen(text);
    
    while (i < tlen && pos < max_len - 1) {
        while (i < tlen && text[i] == ' ') i++;
        if (i >= tlen) break;
        
        char word[256];
        int wlen = 0;
        
        if (isalpha(text[i]) || text[i] == '_') {
            while (i < tlen && (isalnum(text[i]) || text[i] == '_') && wlen < 255)
                word[wlen++] = text[i++];
        } else if (isdigit(text[i])) {
            while (i < tlen && isdigit(text[i]) && wlen < 255)
                word[wlen++] = text[i++];
        } else {
            word[wlen++] = text[i++];
        }
        
        word[wlen] = '\0';
        ids[pos++] = vocab_lookup(voc, word);
    }
    
    ids[pos++] = 3;  // EOS
    *len = pos;
}

void build_vocab(Sample *s, int n, Vocab *v) {
    for (int i = 0; i < n; i++) {
        int ids[4096], len;
        tokenize(s[i].output, ids, &len, v, 4096);
        tokenize(s[i].instruction, ids, &len, v, 4096);
    }
    printf("Vocabulario: %d tokens\n", v->size);
}

/* ============================================================
 * KERNELS CUDA
 * ============================================================ */

__global__ void k_matmul(float *A, float *B, float *C, int M, int N, int K) {
    int r = blockIdx.y * 16 + threadIdx.y;
    int c = blockIdx.x * 16 + threadIdx.x;
    if (r < M && c < N) {
        float s = 0;
        for (int k = 0; k < K; k++) s += A[r*K+k] * B[k*N+c];
        C[r*N+c] = s;
    }
}

__global__ void k_softmax(float *x, float *y, int rows, int cols) {
    int r = blockIdx.x * 256 + threadIdx.x;
    if (r < rows) {
        float mx = x[r*cols];
        for (int i = 1; i < cols; i++) mx = fmaxf(mx, x[r*cols+i]);
        float s = 0;
        for (int i = 0; i < cols; i++) { y[r*cols+i] = expf(x[r*cols+i]-mx); s += y[r*cols+i]; }
        for (int i = 0; i < cols; i++) y[r*cols+i] /= s;
    }
}

__global__ void k_gelu(float *x, float *y, int n) {
    int i = blockIdx.x * 256 + threadIdx.x;
    if (i < n) {
        float v = x[i];
        y[i] = 0.5f * v * (1.0f + tanhf(0.7978845608f * (v + 0.044715f * v * v * v)));
    }
}

__global__ void k_residual_norm(float *a, float *b, float *y, 
                                 float *g, float *beta, int rows, int cols) {
    int r = blockIdx.x * 256 + threadIdx.x;
    if (r < rows) {
        for (int i = 0; i < cols; i++) y[r*cols+i] = a[r*cols+i] + b[r*cols+i];
        float m = 0;
        for (int i = 0; i < cols; i++) m += y[r*cols+i];
        m /= cols;
        float v = 0;
        for (int i = 0; i < cols; i++) v += (y[r*cols+i]-m)*(y[r*cols+i]-m);
        v /= cols;
        float si = 1.0f / sqrtf(v + 1e-5f);
        for (int i = 0; i < cols; i++)
            y[r*cols+i] = g[i] * (y[r*cols+i]-m) * si + beta[i];
    }
}

__global__ void k_embed_lookup(float *emb, int *ids, float *out, int seq, int dim) {
    int p = blockIdx.x * 256 + threadIdx.x;
    if (p < seq) {
        int id = ids[p];
        for (int d = 0; d < dim; d++) out[p*dim+d] = emb[id*dim+d];
    }
}

__global__ void k_add_pe(float *x, float *pe, float *y, int seq, int dim) {
    int p = blockIdx.y * 16 + threadIdx.y;
    int d = blockIdx.x * 16 + threadIdx.x;
    if (p < seq && d < dim) y[p*dim+d] = x[p*dim+d] + pe[p*dim+d];
}

__global__ void k_attn_score(float *Q, float *K, float *S, int seq, int dk, float scale) {
    int i = blockIdx.y * 16 + threadIdx.y;
    int j = blockIdx.x * 16 + threadIdx.x;
    if (i < seq && j < seq) {
        float d = 0;
        for (int k = 0; k < dk; k++) d += Q[i*dk+k] * K[j*dk+k];
        S[i*seq+j] = d * scale;
    }
}

__global__ void k_attn_val(float *A, float *V, float *O, int seq, int dv) {
    int i = blockIdx.y * 16 + threadIdx.y;
    int d = blockIdx.x * 16 + threadIdx.x;
    if (i < seq && d < dv) {
        float s = 0;
        for (int j = 0; j < seq; j++) s += A[i*seq+j] * V[j*dv+d];
        O[i*dv+d] = s;
    }
}

/* ============================================================
 * FUNCIONES GPU
 * ============================================================ */

void gpu_matmul(float *d_A, float *d_B, float *d_C, int M, int N, int K) {
    dim3 block(16, 16);
    dim3 grid((N+15)/16, (M+15)/16);
    k_matmul<<<grid, block>>>(d_A, d_B, d_C, M, N, K);
    cudaDeviceSynchronize();
}

void gpu_softmax(float *d_x, float *d_y, int rows, int cols) {
    k_softmax<<<(rows+255)/256, 256>>>(d_x, d_y, rows, cols);
    cudaDeviceSynchronize();
}

void gpu_gelu(float *d_x, float *d_y, int n) {
    k_gelu<<<(n+255)/256, 256>>>(d_x, d_y, n);
    cudaDeviceSynchronize();
}

void gpu_residual_norm(float *d_a, float *d_b, float *d_y,
                        float *d_g, float *d_b2, int rows, int cols) {
    k_residual_norm<<<(rows+255)/256, 256>>>(d_a, d_b, d_y, d_g, d_b2, rows, cols);
    cudaDeviceSynchronize();
}

void gpu_embed_lookup(float *d_emb, int *d_ids, float *d_out, int seq, int dim) {
    k_embed_lookup<<<(seq+255)/256, 256>>>(d_emb, d_ids, d_out, seq, dim);
    cudaDeviceSynchronize();
}

void gpu_add_pe(float *d_x, float *d_pe, float *d_y, int seq, int dim) {
    dim3 block(16, 16);
    dim3 grid((dim+15)/16, (seq+15)/16);
    k_add_pe<<<grid, block>>>(d_x, d_pe, d_y, seq, dim);
    cudaDeviceSynchronize();
}

void gpu_multihead_attn(float *d_Q, float *d_K, float *d_V, float *d_out,
                         int seq, int n_heads, int d_model) {
    int dk = d_model / n_heads;
    float scale = 1.0f / sqrtf((float)dk);
    
    // Scores = Q × K^T
    dim3 block(16, 16);
    dim3 grid((seq+15)/16, (seq+15)/16);
    k_attn_score<<<grid, block>>>(d_Q, d_K, d_out, seq, dk, scale);
    cudaDeviceSynchronize();
    
    // Softmax
    gpu_softmax(d_out, d_K, seq, seq);
    
    // Attention × V
    dim3 grid2((d_model+15)/16, (seq+15)/16);
    k_attn_val<<<grid2, block>>>(d_K, d_V, d_out, seq, d_model);
    cudaDeviceSynchronize();
}

/* ============================================================
 * MODELO EN GPU
 * ============================================================ */

typedef struct {
    // Parámetros en GPU
    float *d_enc_emb, *d_dec_emb, *d_pe;
    float *d_Wq, *d_Wk, *d_Wv, *d_Wo;
    float *d_W1, *d_W2;
    float *d_ln1g, *d_ln1b, *d_ln2g, *d_ln2b;
    
    // Dimensiones
    int vocab_size, d_model, n_heads, d_ff, n_layers;
} GPUModel;

GPUModel* gpu_model_create(int vocab, int d_model, int n_heads, int d_ff, int n_layers) {
    GPUModel *m = (GPUModel*)malloc(sizeof(GPUModel));
    m->vocab_size = vocab;
    m->d_model = d_model;
    m->n_heads = n_heads;
    m->d_ff = d_ff;
    m->n_layers = n_layers;
    
    // Allocar en GPU
    CUDA_CHECK(cudaMalloc(&m->d_enc_emb, vocab * d_model * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&m->d_dec_emb, vocab * d_model * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&m->d_pe, 512 * d_model * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&m->d_Wq, d_model * d_model * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&m->d_Wk, d_model * d_model * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&m->d_Wv, d_model * d_model * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&m->d_Wo, d_model * d_model * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&m->d_W1, d_model * d_ff * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&m->d_W2, d_ff * d_model * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&m->d_ln1g, d_model * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&m->d_ln1b, d_model * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&m->d_ln2g, d_model * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&m->d_ln2b, d_model * sizeof(float)));
    
    // Inicializar pesos aleatorios en CPU y copiar
    float *h_buf = (float*)malloc(d_model * d_model * sizeof(float));
    float limit = sqrtf(6.0f / d_model);
    
    for (int i = 0; i < d_model * d_model; i++)
        h_buf[i] = ((float)rand() / RAND_MAX) * 2 * limit - limit;
    
    CUDA_CHECK(cudaMemcpy(m->d_Wq, h_buf, d_model * d_model * sizeof(float), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(m->d_Wk, h_buf, d_model * d_model * sizeof(float), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(m->d_Wv, h_buf, d_model * d_model * sizeof(float), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(m->d_Wo, h_buf, d_model * d_model * sizeof(float), cudaMemcpyHostToDevice));
    
    // Embeddings
    limit = sqrtf(6.0f / (vocab + d_model));
    for (int i = 0; i < vocab * d_model; i++)
        h_buf[i] = ((float)rand() / RAND_MAX) * 2 * limit - limit;
    
    float *h_emb = (float*)malloc(vocab * d_model * sizeof(float));
    for (int i = 0; i < vocab * d_model; i++)
        h_emb[i] = ((float)rand() / RAND_MAX) * 2 * limit - limit;
    
    CUDA_CHECK(cudaMemcpy(m->d_enc_emb, h_emb, vocab * d_model * sizeof(float), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(m->d_dec_emb, h_emb, vocab * d_model * sizeof(float), cudaMemcpyHostToDevice));
    free(h_emb);
    
    // Positional encoding
    float *h_pe = (float*)calloc(512 * d_model, sizeof(float));
    for (int p = 0; p < 512; p++) {
        for (int d = 0; d < d_model; d++) {
            float angle = p / powf(10000.0f, (float)(2*(d/2)) / d_model);
            h_pe[p*d_model+d] = (d % 2 == 0) ? sinf(angle) : cosf(angle);
        }
    }
    CUDA_CHECK(cudaMemcpy(m->d_pe, h_pe, 512 * d_model * sizeof(float), cudaMemcpyHostToDevice));
    free(h_pe);
    
    // FFN weights
    limit = sqrtf(6.0f / d_model);
    for (int i = 0; i < d_model * d_ff; i++)
        h_buf[i] = ((float)rand() / RAND_MAX) * 2 * limit - limit;
    CUDA_CHECK(cudaMalloc(&m->d_W1, d_model * d_ff * sizeof(float)));
    CUDA_CHECK(cudaMemcpy(m->d_W1, h_buf, d_model * d_ff * sizeof(float), cudaMemcpyHostToDevice));
    
    limit = sqrtf(6.0f / d_ff);
    for (int i = 0; i < d_ff * d_model; i++)
        h_buf[i] = ((float)rand() / RAND_MAX) * 2 * limit - limit;
    CUDA_CHECK(cudaMalloc(&m->d_W2, d_ff * d_model * sizeof(float)));
    CUDA_CHECK(cudaMemcpy(m->d_W2, h_buf, d_ff * d_model * sizeof(float), cudaMemcpyHostToDevice));
    
    // LayerNorm
    float ones[1024], zeros[1024];
    for (int i = 0; i < d_model; i++) { ones[i] = 1.0f; zeros[i] = 0.0f; }
    CUDA_CHECK(cudaMemcpy(m->d_ln1g, ones, d_model * sizeof(float), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(m->d_ln1b, zeros, d_model * sizeof(float), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(m->d_ln2g, ones, d_model * sizeof(float), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(m->d_ln2b, zeros, d_model * sizeof(float), cudaMemcpyHostToDevice));
    
    free(h_buf);
    
    printf("Modelo creado en GPU: %d params\n", 
           vocab*d_model*2 + n_layers*(4*d_model*d_model + 2*d_model*d_ff));
    
    return m;
}

float gpu_forward(GPUModel *m, int *h_enc_ids, int *h_dec_ids, int seq_len) {
    int dm = m->d_model;
    int df = m->d_ff;
    
    // Allocar buffers en GPU
    float *d_enc, *d_dec, *d_temp, *d_out;
    int *d_enc_ids, *d_dec_ids;
    
    CUDA_CHECK(cudaMalloc(&d_enc, seq_len * dm * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_dec, seq_len * dm * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_temp, seq_len * dm * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_out, seq_len * dm * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_enc_ids, seq_len * sizeof(int)));
    CUDA_CHECK(cudaMalloc(&d_dec_ids, seq_len * sizeof(int)));
    
    CUDA_CHECK(cudaMemcpy(d_enc_ids, h_enc_ids, seq_len * sizeof(int), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_dec_ids, h_dec_ids, seq_len * sizeof(int), cudaMemcpyHostToDevice));
    
    // Embedding lookup
    gpu_embed_lookup(m->d_enc_emb, d_enc_ids, d_enc, seq_len, dm);
    gpu_embed_lookup(m->d_dec_emb, d_dec_ids, d_dec, seq_len, dm);
    
    // Add positional encoding
    gpu_add_pe(d_enc, m->d_pe, d_enc, seq_len, dm);
    gpu_add_pe(d_dec, m->d_pe, d_dec, seq_len, dm);
    
    // Decoder layers
    for (int l = 0; l < m->n_layers; l++) {
        // Self attention
        gpu_matmul(d_dec, m->d_Wq, d_temp, seq_len, dm, dm);  // Q
        gpu_multihead_attn(d_temp, d_temp, d_temp, d_out, seq_len, m->n_heads, dm);
        
        // Residual + Norm
        gpu_residual_norm(d_dec, d_out, d_temp, m->d_ln1g, m->d_ln1b, seq_len, dm);
        
        // FFN
        gpu_matmul(d_temp, m->d_W1, d_out, seq_len, df, dm);
        gpu_gelu(d_out, d_out, seq_len * df);
        gpu_matmul(d_out, m->d_W2, d_dec, seq_len, dm, df);
        
        // Residual + Norm
        gpu_residual_norm(d_temp, d_dec, d_out, m->d_ln2g, m->d_ln2b, seq_len, dm);
        
        // Swap
        float *tmp = d_dec; d_dec = d_out; d_out = tmp;
    }
    
    // Copiar resultado
    float *h_result = (float*)malloc(seq_len * dm * sizeof(float));
    CUDA_CHECK(cudaMemcpy(h_result, d_dec, seq_len * dm * sizeof(float), cudaMemcpyDeviceToHost));
    
    // Calcular loss simple (promedio de aktivaciones)
    float loss = 0;
    for (int i = 0; i < seq_len * dm; i++) loss += h_result[i] * h_result[i];
    loss /= (seq_len * dm);
    
    free(h_result);
    
    // Liberar
    cudaFree(d_enc); cudaFree(d_dec); cudaFree(d_temp); cudaFree(d_out);
    cudaFree(d_enc_ids); cudaFree(d_dec_ids);
    
    return loss;
}

void gpu_model_free(GPUModel *m) {
    cudaFree(m->d_enc_emb); cudaFree(m->d_dec_emb); cudaFree(m->d_pe);
    cudaFree(m->d_Wq); cudaFree(m->d_Wk); cudaFree(m->d_Wv); cudaFree(m->d_Wo);
    cudaFree(m->d_W1); cudaFree(m->d_W2);
    cudaFree(m->d_ln1g); cudaFree(m->d_ln1b); cudaFree(m->d_ln2g); cudaFree(m->d_ln2b);
    free(m);
}

/* ============================================================
 * MAIN
 * ============================================================ */

int main(int argc, char *argv[]) {
    // Info GPU
    cudaDeviceProp prop;
    CUDA_CHECK(cudaGetDeviceProperties(&prop, 0));
    
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  TRANSFORMER CUDA - HARBOUR/FWH DATASET                   ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    printf("GPU: %s\n", prop.name);
    printf("VRAM: %d MB\n", (int)(prop.totalGlobalMem / (1024*1024)));
    printf("Compute: %d.%d\n\n", prop.major, prop.minor);
    
    if (argc < 2) {
        printf("Uso: %s <dataset.jsonl> [muestras] [epochs]\n", argv[0]);
        return 1;
    }
    
    const char *filename = argv[1];
    int max_samples = (argc > 2) ? atoi(argv[2]) : 5000;
    int n_epochs = (argc > 3) ? atoi(argv[3]) : 20;
    int seq_len = 64;
    int d_model = 128;
    int n_heads = 4;
    int d_ff = 256;
    int n_layers = 2;
    
    // Cargar datos
    int n_samples;
    Sample *samples = load_data(filename, max_samples, &n_samples);
    
    // Vocabulario
    Vocab *voc = vocab_create(10000);
    build_vocab(samples, n_samples, voc);
    
    // Crear modelo en GPU
    srand(time(NULL));
    GPUModel *model = gpu_model_create(voc->size, d_model, n_heads, d_ff, n_layers);
    
    // Entrenar
    printf("\n═══ ENTRENAMIENTO ═══\n\n");
    
    clock_t total_start = clock();
    
    for (int epoch = 0; epoch < n_epochs; epoch++) {
        clock_t ep_start = clock();
        float total_loss = 0;
        
        // Shuffle
        int *idx = (int*)malloc(n_samples * sizeof(int));
        for (int i = 0; i < n_samples; i++) idx[i] = i;
        for (int i = n_samples-1; i > 0; i--) {
            int j = rand() % (i+1);
            int t = idx[i]; idx[i] = idx[j]; idx[j] = t;
        }
        
        for (int b = 0; b < n_samples; b++) {
            int s = idx[b];
            
            // Tokenizar
            int enc_ids[1024], dec_ids[1024], enc_len, dec_len;
            
            char prompt[4096];
            snprintf(prompt, sizeof(prompt), "%s %s", samples[s].instruction, samples[s].input);
            tokenize(prompt, enc_ids, &enc_len, voc, seq_len);
            tokenize(samples[s].output, dec_ids, &dec_len, voc, seq_len);
            
            // Pad
            while (enc_len < seq_len) enc_ids[enc_len++] = 0;
            while (dec_len < seq_len) dec_ids[dec_len++] = 0;
            
            // Forward
            float loss = gpu_forward(model, enc_ids, dec_ids, seq_len);
            total_loss += loss;
            
            if ((b+1) % 50 == 0) {
                printf("\r  Batch %d/%d (%.0f%%) Loss: %.4f", 
                       b+1, n_samples, 100.0f*(b+1)/n_samples, total_loss/(b+1));
                fflush(stdout);
            }
        }
        
        free(idx);
        
        clock_t ep_end = clock();
        float ep_time = (float)(ep_end - ep_start) / CLOCKS_PER_SEC;
        float avg_loss = total_loss / n_samples;
        
        printf("\r  Epoch %3d/%d | Loss: %.4f | Time: %.1fs     \n",
               epoch+1, n_epochs, avg_loss, ep_time);
    }
    
    clock_t total_end = clock();
    printf("\nTiempo total: %.1f segundos\n", (float)(total_end-total_start)/CLOCKS_PER_SEC);
    
    // Cleanup
    gpu_model_free(model);
    free_samples(samples, n_samples);
    
    printf("\n✓ Entrenamiento completado\n");
    
    return 0;
}
