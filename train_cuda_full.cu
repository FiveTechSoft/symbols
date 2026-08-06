/**
 * train_cuda_full.cu - Transformer CUDA Training
 * Pre-allocated GPU buffers, real backprop, Adam optimizer
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <cuda_runtime.h>

#define CUDACHECK(call) { cudaError_t e=call; if(e!=cudaSuccess){fprintf(stderr,"CUDA Err %d: %s\n",__LINE__,cudaGetErrorString(e));if(g_log){fprintf(g_log,"CUDA Err %d: %s\n",__LINE__,cudaGetErrorString(e));fflush(g_log);}fflush(stderr);exit(1);}}
#define DM 128
#define DF 256
#define SQ 32
#define LR 0.000005f
#define B1 0.9f
#define B2 0.999f
#define EPS 1e-8f

FILE *g_log=NULL;
void log_msg(const char *m){printf("%s",m);fflush(stdout);if(g_log){fprintf(g_log,"%s",m);fflush(g_log);}}
char g_b[512];

/* ---- Vocab ---- */
typedef struct{char**w;int n;}Voc;
Voc* voc_new(){Voc*v=(Voc*)malloc(sizeof(Voc));v->w=(char**)malloc(8000*sizeof(char*));v->n=4;v->w[0]=strdup("<PAD>");v->w[1]=strdup("<UNK>");v->w[2]=strdup("<BOS>");v->w[3]=strdup("<EOS>");return v;}
int voc_add(Voc*v,char*s){for(int i=0;i<v->n;i++)if(!strcmp(v->w[i],s))return i;    if(v->n>=4096)return 1;v->w[v->n]=strdup(s);return v->n++;}
void tokenize(char*text,int*ids,int*len,Voc*v){
    int p=0,i=0;ids[p++]=2;int tl=(int)strlen(text);
    while(i<tl&&p<SQ-1){while(i<tl&&text[i]==' ')i++;if(i>=tl)break;
    char wd[64];int wl=0;
    if(isalpha(text[i]))while(i<tl&&(isalnum(text[i])||text[i]=='_')&&wl<63)wd[wl++]=text[i++];
    else if(isdigit(text[i]))while(i<tl&&isdigit(text[i])&&wl<63)wd[wl++]=text[i++];
    else wd[wl++]=text[i++];
    wd[wl]=0;ids[p++]=voc_add(v,wd);}ids[p++]=3;*len=p;
}

/* ---- Dataset ---- */
typedef struct{int*enc,*dec,*tgt;int len,dlen;}Smp;
Smp* load_data(const char*file,Voc*v,int*cnt,int mx){
    FILE*f=fopen(file,"r");if(!f){printf("Cannot open %s\n",file);exit(1);}
    Smp*s=(Smp*)malloc(mx*sizeof(Smp));char ln[65536];*cnt=0;
    while(fgets(ln,sizeof(ln),f)&&*cnt<mx){
        if(ln[0]=='\n')continue;
        char*inst=strstr(ln,"\"instruction\"");char*out=strstr(ln,"\"output\"");
        if(!inst||!out)continue;
        char si[2048]="",so[4096]="";
        inst=strchr(inst,':');if(inst){inst=strchr(inst+1,'"');if(inst){inst++;char*e=strchr(inst,'"');if(e){int l=e-inst;if(l>2047)l=2047;memcpy(si,inst,l);si[l]=0;}}}
        out=strchr(out,':');if(out){out=strchr(out+1,'"');if(out){out++;char*e=strchr(out,'"');if(e){int l=e-out;if(l>4095)l=4095;memcpy(so,out,l);so[l]=0;}}}
        if(!so[0])continue;
        int enc[SQ],dec[SQ],el,dl;
        tokenize(si,enc,&el,v);tokenize(so,dec,&dl,v);
        s[*cnt].enc=(int*)malloc(el*sizeof(int));s[*cnt].dec=(int*)malloc(dl*sizeof(int));s[*cnt].tgt=(int*)malloc(dl*sizeof(int));s[*cnt].len=el;
        memcpy(s[*cnt].enc,enc,el*sizeof(int));memcpy(s[*cnt].dec,dec,dl*sizeof(int));
        for(int i=0;i<dl-1;i++)s[*cnt].tgt[i]=dec[i+1];s[*cnt].tgt[dl-1]=3;s[*cnt].len=el;s[*cnt].dlen=dl;(*cnt)++;
    }fclose(f);return s;
}

/* ============ KERNELS ============ */
__global__ void k_matmul(float*A,float*B,float*C,int M,int N,int K){
    int r=blockIdx.y*16+threadIdx.y,c=blockIdx.x*16+threadIdx.x;
    if(r<M&&c<N){float s=0;for(int k=0;k<K;k++)s+=A[r*K+k]*B[k*N+c];C[r*N+c]=s;}
}
__global__ void k_softmax(float*x,float*y,int R,int C){
    int r=blockIdx.x*256+threadIdx.x;if(r<R){float mx=x[r*C];for(int i=1;i<C;i++)mx=fmaxf(mx,x[r*C+i]);float s=0;for(int i=0;i<C;i++){float e=expf(x[r*C+i]-mx);y[r*C+i]=e;s+=e;}for(int i=0;i<C;i++)y[r*C+i]/=s;}
}
__global__ void k_gelu(float*x,float*y,int n){int i=blockIdx.x*256+threadIdx.x;if(i<n){float v=x[i];y[i]=.5f*v*(1.f+tanhf(.7978845608f*(v+.044715f*v*v*v)));}}
__global__ void k_layernorm(float*x,float*y,float*g,float*b,int R,int C){
    int r=blockIdx.x*256+threadIdx.x;if(r<R){float m=0;for(int i=0;i<C;i++)m+=x[r*C+i];m/=C;float v=0;for(int i=0;i<C;i++)v+=(x[r*C+i]-m)*(x[r*C+i]-m);v/=C;float s=1.f/sqrtf(v+1e-5f);for(int i=0;i<C;i++)y[r*C+i]=g[i]*(x[r*C+i]-m)*s+b[i];}
}
__global__ void k_embed(float*E,int*ids,float*O,int S,int D,int V){
    int p=blockIdx.x*256+threadIdx.x;if(p<S){int id=ids[p];if(id<0)id=0;if(id>=V)id=V-1;for(int d=0;d<D;d++)O[p*D+d]=E[id*D+d];}
}
__global__ void k_addpe(float*x,float*pe,int S,int D){int p=blockIdx.y*16+threadIdx.y,d=blockIdx.x*16+threadIdx.x;if(p<S&&d<D)x[p*D+d]+=pe[p*D+d];}
__global__ void k_add(float*a,float*b,float*c,int n){int i=blockIdx.x*256+threadIdx.x;if(i<n)c[i]=a[i]+b[i];}
/* Backward */
__global__ void k_ce_grad(float*P,float*dL,int*tgt,int S,int V){
    int r=blockIdx.x*256+threadIdx.x;if(r<S){int t=tgt[r];for(int c=0;c<V;c++){dL[r*V+c]=P[r*V+c];if(c==t)dL[r*V+c]-=1.f;}}
}
__global__ void k_softmax_g(float*P,float*dY,float*dX,int R,int C){
    int r=blockIdx.x*256+threadIdx.x;if(r<R){float dot=0;for(int i=0;i<C;i++)dot+=dY[r*C+i]*P[r*C+i];for(int i=0;i<C;i++)dX[r*C+i]=P[r*C+i]*(dY[r*C+i]-dot);}
}
__global__ void k_gelu_g(float*x,float*dY,float*dX,int n){
    int i=blockIdx.x*256+threadIdx.x;if(i<n){float v=x[i];float a=.7978845608f*(v+.044715f*v*v*v);float t=tanhf(a);dX[i]=dY[i]*(.5f*(1.f+t)+.5f*v*(1.f-t*t)*.7978845608f*(1.f+.134145f*v*v));}
}
__global__ void k_ln_g(float*x,float*dY,float*g,float*dX,float*dG,float*dB,int R,int C){
    int r=blockIdx.x*256+threadIdx.x;
    if(r<R){
        float m=0;for(int i=0;i<C;i++)m+=x[r*C+i];m/=C;
        float v=0;for(int i=0;i<C;i++){float d=x[r*C+i]-m;v+=d*d;}v/=C;
        float s=1.f/sqrtf(v+1e-5f);
        float dot=0,dot2=0;
        for(int i=0;i<C;i++){
            float xhat=(x[r*C+i]-m)*s;
            atomicAdd(&dG[i],dY[r*C+i]*xhat);
            atomicAdd(&dB[i],dY[r*C+i]);
            dot+=dY[r*C+i]*g[i];
            dot2+=dY[r*C+i]*g[i]*xhat;
        }
        /* Correct layernorm backward: dx = s*(g*dY - dot/C - xhat*dot2/C) */
        for(int i=0;i<C;i++){
            float xhat=(x[r*C+i]-m)*s;
            dX[r*C+i]=s*(g[i]*dY[r*C+i]-dot/C-xhat*dot2/C);
        }
    }
}
/* dA += dC * B : for C=A*B [M,N]=[M,K]*[K,N] */
__global__ void k_gA(float*dC,float*B,float*dA,int M,int N,int K){
    int r=blockIdx.y*16+threadIdx.y,c=blockIdx.x*16+threadIdx.x;
    if(r<M&&c<K){float s=0;for(int n=0;n<N;n++)s+=dC[r*N+n]*B[c*N+n];dA[r*K+c]+=s;}
}
/* dB += A^T * dC */
__global__ void k_gB(float*A,float*dC,float*dB,int M,int N,int K){
    int r=blockIdx.y*16+threadIdx.y,c=blockIdx.x*16+threadIdx.x;
    if(r<K&&c<N){float s=0;for(int m=0;m<M;m++)s+=A[m*K+r]*dC[m*N+c];dB[r*N+c]+=s;}
}
/* Scatter embed grad */
__global__ void k_eg(float*dE,float*dO,int*ids,int S,int D,int V){
    int p=blockIdx.x*256+threadIdx.x;if(p<S){int id=ids[p];if(id<0)id=0;if(id>=V)id=V-1;for(int d=0;d<D;d++)atomicAdd(&dE[id*D+d],dO[p*D+d]);}
}
/* Adam */
__global__ void k_adam_wd(float*w,float*g,float*mv,float*vv,float lr,float b1,float b2,float eps,float bt1,float bt2,float wd,int n){
    int i=blockIdx.x*256+threadIdx.x;if(i<n){
        float gi=g[i]+wd*w[i];
        if(gi!=gi) gi=0.f;
        float m=b1*mv[i]+(1.f-b1)*gi;
        float v=b2*vv[i]+(1.f-b2)*gi*gi;
        if(m!=m) m=0.f;
        if(v!=v) v=0.f;
        mv[i]=m; vv[i]=v;
        float wnew=w[i]-lr*(m/(1.f-bt1))/(sqrtf(v/(1.f-bt2))+eps);
        if(wnew==wnew && wnew*0.f==0.f) w[i]=wnew;
    }
}
/* Gradient clipping: cap max absolute value */
__global__ void k_clip(float*x,float mx,int n){int i=blockIdx.x*256+threadIdx.x;if(i<n){if(x[i]>mx)x[i]=mx;else if(x[i]<-mx)x[i]=-mx;}}
/* Compute sum of squares */
__global__ void k_sum_sq(float*x,float*out,int n){
    __shared__ float s[256]; int tid=threadIdx.x;
    float v=0; for(int i=tid;i<n;i+=256){float xi=x[i];v+=xi*xi;}
    s[tid]=v; __syncthreads();
    for(int s2=128;s2>=1;s2>>=1){if(tid<s2)s[tid]+=s[tid+s2];__syncthreads();}
    if(tid==0)atomicAdd(out,s[0]);
}
/* Scale all elements by a factor */
__global__ void k_scale_all(float*x,float f,int n){int i=blockIdx.x*256+threadIdx.x;if(i<n)x[i]*=f;}

/* ============ MODEL ============ */
typedef struct{
    float*d_emb,*d_pe,*dWq,*dWk,*dWv,*dWo,*dW1,*dW2,*dWout;
    float*dln1g,*dln1b,*dln2g,*dln2b;
    float*mWq,*vWq,*mWk,*vWk,*mWv,*vWv,*mWo,*vWo;
    float*mW1,*vW1,*mW2,*vW2,*mWo2,*vWo2;
    float*mln1g,*vln1g,*mln1b,*vln1b,*mln2g,*vln2g,*mln2b,*vln2b;
    float*memb,*vemb;
    /* Pre-allocated forward activations */
    float *af_enc_e,*af_dec_e,*af_Q,*af_K,*af_V;
    float *af_asc,*af_apc,*af_ao;
    float *af_r1,*af_ln1;
    float *af_f1,*af_f1g,*af_f2;
    float *af_r2,*af_ln2;
    float *af_logits,*af_probs;
    /* Pre-allocated gradients */
    float *ag_dL,*ag_dl2,*ag_dr2,*ag_df2,*ag_df1g,*ag_df1;
    float *ag_dr1,*ag_dl1,*ag_dao;
    float *ag_dQ,*ag_dK,*ag_dV,*ag_dde;
    float *ag_Wq,*ag_Wk,*ag_Wv,*ag_Wo,*ag_W1,*ag_W2,*ag_Wo2;
    float *ag_ln1g,*ag_ln1b,*ag_ln2g,*ag_ln2b,*ag_emb;
    /* GPU IDs */
    int *gd_enc,*gd_dec,*gd_tgt;
    float *d_normbuf; /* single float for gradient norm computation */
    int vocab;
}Model;

Model* model_create(int voc){
    Model*m=(Model*)calloc(1,sizeof(Model));m->vocab=voc;
    size_t sd=SQ*DM*sizeof(float),sf=SQ*DF*sizeof(float),ss=SQ*SQ*sizeof(float);
    size_t sq=DM*DM*sizeof(float),s1=DM*DF*sizeof(float),s2=DF*DM*sizeof(float),so=voc*DM*sizeof(float),sl=DM*sizeof(float),se=voc*DM*sizeof(float),sp=SQ*DM*sizeof(float);

    /* Weights */
    CUDACHECK(cudaMalloc(&m->d_emb,se));CUDACHECK(cudaMalloc(&m->d_pe,sp));
    CUDACHECK(cudaMalloc(&m->dWq,sq));CUDACHECK(cudaMalloc(&m->dWk,sq));
    CUDACHECK(cudaMalloc(&m->dWv,sq));CUDACHECK(cudaMalloc(&m->dWo,sq));
    CUDACHECK(cudaMalloc(&m->dW1,s1));CUDACHECK(cudaMalloc(&m->dW2,s2));
    CUDACHECK(cudaMalloc(&m->dWout,so));
    CUDACHECK(cudaMalloc(&m->dln1g,sl));CUDACHECK(cudaMalloc(&m->dln1b,sl));
    CUDACHECK(cudaMalloc(&m->dln2g,sl));CUDACHECK(cudaMalloc(&m->dln2b,sl));
    /* Adam */
    CUDACHECK(cudaMalloc(&m->mWq,sq));CUDACHECK(cudaMalloc(&m->vWq,sq));
    CUDACHECK(cudaMalloc(&m->mWk,sq));CUDACHECK(cudaMalloc(&m->vWk,sq));
    CUDACHECK(cudaMalloc(&m->mWv,sq));CUDACHECK(cudaMalloc(&m->vWv,sq));
    CUDACHECK(cudaMalloc(&m->mWo,sq));CUDACHECK(cudaMalloc(&m->vWo,sq));
    CUDACHECK(cudaMalloc(&m->mW1,s1));CUDACHECK(cudaMalloc(&m->vW1,s1));
    CUDACHECK(cudaMalloc(&m->mW2,s2));CUDACHECK(cudaMalloc(&m->vW2,s2));
    CUDACHECK(cudaMalloc(&m->mWo2,so));CUDACHECK(cudaMalloc(&m->vWo2,so));
    CUDACHECK(cudaMalloc(&m->mln1g,sl));CUDACHECK(cudaMalloc(&m->vln1g,sl));
    CUDACHECK(cudaMalloc(&m->mln1b,sl));CUDACHECK(cudaMalloc(&m->vln1b,sl));
    CUDACHECK(cudaMalloc(&m->mln2g,sl));CUDACHECK(cudaMalloc(&m->vln2g,sl));
    CUDACHECK(cudaMalloc(&m->mln2b,sl));CUDACHECK(cudaMalloc(&m->vln2b,sl));
    CUDACHECK(cudaMalloc(&m->memb,se));CUDACHECK(cudaMalloc(&m->vemb,se));

    /* Forward activations */
    CUDACHECK(cudaMalloc(&m->af_enc_e,sd));CUDACHECK(cudaMalloc(&m->af_dec_e,sd));
    CUDACHECK(cudaMalloc(&m->af_Q,sd));CUDACHECK(cudaMalloc(&m->af_K,sd));CUDACHECK(cudaMalloc(&m->af_V,sd));
    CUDACHECK(cudaMalloc(&m->af_asc,ss));CUDACHECK(cudaMalloc(&m->af_apc,ss));
    CUDACHECK(cudaMalloc(&m->af_ao,sd));CUDACHECK(cudaMalloc(&m->af_r1,sd));CUDACHECK(cudaMalloc(&m->af_ln1,sd));
    CUDACHECK(cudaMalloc(&m->af_f1,sf));CUDACHECK(cudaMalloc(&m->af_f1g,sf));CUDACHECK(cudaMalloc(&m->af_f2,sd));
    CUDACHECK(cudaMalloc(&m->af_r2,sd));CUDACHECK(cudaMalloc(&m->af_ln2,sd));
    CUDACHECK(cudaMalloc(&m->af_logits,SQ*voc*sizeof(float)));CUDACHECK(cudaMalloc(&m->af_probs,SQ*voc*sizeof(float)));

    /* Gradients */
    CUDACHECK(cudaMalloc(&m->ag_dL,SQ*voc*sizeof(float)));CUDACHECK(cudaMalloc(&m->ag_dl2,sd));
    CUDACHECK(cudaMalloc(&m->ag_dr2,sd));CUDACHECK(cudaMalloc(&m->ag_df2,sd));
    CUDACHECK(cudaMalloc(&m->ag_df1g,sf));CUDACHECK(cudaMalloc(&m->ag_df1,sf));
    CUDACHECK(cudaMalloc(&m->ag_dr1,sd));CUDACHECK(cudaMalloc(&m->ag_dl1,sd));CUDACHECK(cudaMalloc(&m->ag_dao,sd));
    CUDACHECK(cudaMalloc(&m->ag_dQ,sd));CUDACHECK(cudaMalloc(&m->ag_dK,sd));CUDACHECK(cudaMalloc(&m->ag_dV,sd));CUDACHECK(cudaMalloc(&m->ag_dde,sd));
    CUDACHECK(cudaMalloc(&m->ag_Wq,sq));CUDACHECK(cudaMalloc(&m->ag_Wk,sq));
    CUDACHECK(cudaMalloc(&m->ag_Wv,sq));CUDACHECK(cudaMalloc(&m->ag_Wo,sq));
    CUDACHECK(cudaMalloc(&m->ag_W1,s1));CUDACHECK(cudaMalloc(&m->ag_W2,s2));CUDACHECK(cudaMalloc(&m->ag_Wo2,so));
    CUDACHECK(cudaMalloc(&m->ag_ln1g,sl));CUDACHECK(cudaMalloc(&m->ag_ln1b,sl));
    CUDACHECK(cudaMalloc(&m->ag_ln2g,sl));CUDACHECK(cudaMalloc(&m->ag_ln2b,sl));
    CUDACHECK(cudaMalloc(&m->ag_emb,se));

    /* GPU index buffers */
    CUDACHECK(cudaMalloc(&m->gd_enc,SQ*sizeof(int)));CUDACHECK(cudaMalloc(&m->gd_dec,SQ*sizeof(int)));CUDACHECK(cudaMalloc(&m->gd_tgt,SQ*sizeof(int)));
    CUDACHECK(cudaMalloc(&m->d_normbuf,sizeof(float)));CUDACHECK(cudaMemset(m->d_normbuf,0,sizeof(float)));

    log_msg("  Allocated all GPU buffers\n");

    /* Init weights */
    int max_w = DM*DF; if(DM*voc>max_w) max_w=DM*voc;
    float*h=(float*)malloc(max_w*sizeof(float));
    float lim=sqrtf(6.f/(DM+DM));for(int i=0;i<DM*DM;i++)h[i]=((float)rand()/RAND_MAX)*2*lim-lim;
    CUDACHECK(cudaMemcpy(m->dWq,h,sq,cudaMemcpyHostToDevice));
    CUDACHECK(cudaMemcpy(m->dWk,h,sq,cudaMemcpyHostToDevice));
    CUDACHECK(cudaMemcpy(m->dWv,h,sq,cudaMemcpyHostToDevice));
    CUDACHECK(cudaMemcpy(m->dWo,h,sq,cudaMemcpyHostToDevice));
    lim=sqrtf(6.f/(DM+DF));for(int i=0;i<DM*DF;i++)h[i]=((float)rand()/RAND_MAX)*2*lim-lim;
    CUDACHECK(cudaMemcpy(m->dW1,h,s1,cudaMemcpyHostToDevice));
    lim=sqrtf(6.f/(DF+DM));for(int i=0;i<DF*DM;i++)h[i]=((float)rand()/RAND_MAX)*2*lim-lim;
    CUDACHECK(cudaMemcpy(m->dW2,h,s2,cudaMemcpyHostToDevice));
    lim=sqrtf(6.f/(DM+voc));for(int i=0;i<DM*voc;i++)h[i]=((float)rand()/RAND_MAX)*2*lim-lim;
    CUDACHECK(cudaMemcpy(m->dWout,h,so,cudaMemcpyHostToDevice));
    float*he=(float*)malloc(se);lim=sqrtf(6.f/(voc+DM));for(int i=0;i<voc*DM;i++)he[i]=((float)rand()/RAND_MAX)*2*lim-lim;
    CUDACHECK(cudaMemcpy(m->d_emb,he,se,cudaMemcpyHostToDevice));
    float*hpe=(float*)calloc(SQ*DM,sizeof(float));
    for(int p=0;p<SQ;p++)for(int d=0;d<DM;d++){float a=p/powf(10000.f,(float)(2*(d/2))/DM);hpe[p*DM+d]=(d%2==0)?sinf(a):cosf(a);}
    CUDACHECK(cudaMemcpy(m->d_pe,hpe,sp,cudaMemcpyHostToDevice));
    float ones[DM],zeros[DM];for(int i=0;i<DM;i++){ones[i]=1.f;zeros[i]=0.f;}
    CUDACHECK(cudaMemcpy(m->dln1g,ones,sl,cudaMemcpyHostToDevice));CUDACHECK(cudaMemcpy(m->dln1b,zeros,sl,cudaMemcpyHostToDevice));
    CUDACHECK(cudaMemcpy(m->dln2g,ones,sl,cudaMemcpyHostToDevice));CUDACHECK(cudaMemcpy(m->dln2b,zeros,sl,cudaMemcpyHostToDevice));

    /* Zero Adam + grads */
    CUDACHECK(cudaMemset(m->mWq,0,sq));CUDACHECK(cudaMemset(m->vWq,0,sq));
    CUDACHECK(cudaMemset(m->mWk,0,sq));CUDACHECK(cudaMemset(m->vWk,0,sq));
    CUDACHECK(cudaMemset(m->mWv,0,sq));CUDACHECK(cudaMemset(m->vWv,0,sq));
    CUDACHECK(cudaMemset(m->mWo,0,sq));CUDACHECK(cudaMemset(m->vWo,0,sq));
    CUDACHECK(cudaMemset(m->mW1,0,s1));CUDACHECK(cudaMemset(m->vW1,0,s1));
    CUDACHECK(cudaMemset(m->mW2,0,s2));CUDACHECK(cudaMemset(m->vW2,0,s2));
    CUDACHECK(cudaMemset(m->mWo2,0,so));CUDACHECK(cudaMemset(m->vWo2,0,so));
    CUDACHECK(cudaMemset(m->memb,0,se));CUDACHECK(cudaMemset(m->vemb,0,se));

    free(h);free(he);free(hpe);
    log_msg("  Weights initialized (Xavier)\n");
    return m;
}

/* ============ TRAINING STEP ============ */
float train_step(Model*m,int step,int*enc,int*dec,int*tgt,int voc){
    int S=SQ,D=DM,F=DF;
    dim3 b16(16,16),gdm((D+15)/16,(S+15)/16),gdf((F+15)/16,(S+15)/16);
    int nemb=(S+255)/256;

    /* Copy indices to GPU */
    CUDACHECK(cudaMemcpy(m->gd_enc,enc,S*sizeof(int),cudaMemcpyHostToDevice));
    CUDACHECK(cudaMemcpy(m->gd_dec,dec,S*sizeof(int),cudaMemcpyHostToDevice));
    CUDACHECK(cudaMemcpy(m->gd_tgt,tgt,S*sizeof(int),cudaMemcpyHostToDevice));

    /* Zero weight gradients */
    CUDACHECK(cudaMemset(m->ag_Wq,0,DM*DM*sizeof(float)));
    CUDACHECK(cudaMemset(m->ag_Wk,0,DM*DM*sizeof(float)));
    CUDACHECK(cudaMemset(m->ag_Wv,0,DM*DM*sizeof(float)));
    CUDACHECK(cudaMemset(m->ag_Wo,0,DM*DM*sizeof(float)));
    CUDACHECK(cudaMemset(m->ag_W1,0,DM*DF*sizeof(float)));
    CUDACHECK(cudaMemset(m->ag_W2,0,DF*DM*sizeof(float)));
    CUDACHECK(cudaMemset(m->ag_Wo2,0,voc*DM*sizeof(float)));
    CUDACHECK(cudaMemset(m->ag_ln1g,0,DM*sizeof(float)));
    CUDACHECK(cudaMemset(m->ag_ln1b,0,DM*sizeof(float)));
    CUDACHECK(cudaMemset(m->ag_ln2g,0,DM*sizeof(float)));
    CUDACHECK(cudaMemset(m->ag_ln2b,0,DM*sizeof(float)));
    CUDACHECK(cudaMemset(m->ag_emb,0,voc*DM*sizeof(float)));

    /* ====== FORWARD ====== */
    k_embed<<<nemb,256>>>(m->d_emb,m->gd_enc,m->af_enc_e,S,D,voc);
    k_addpe<<<gdm,b16>>>(m->af_enc_e,m->d_pe,S,D);
    CUDACHECK(cudaDeviceSynchronize());

    k_embed<<<nemb,256>>>(m->d_emb,m->gd_dec,m->af_dec_e,S,D,voc);
    k_addpe<<<gdm,b16>>>(m->af_dec_e,m->d_pe,S,D);
    CUDACHECK(cudaDeviceSynchronize());

    /* Self-attn Q,K,V */
    k_matmul<<<gdm,b16>>>(m->af_dec_e,m->dWq,m->af_Q,S,D,D);
    k_matmul<<<gdm,b16>>>(m->af_dec_e,m->dWk,m->af_K,S,D,D);
    k_matmul<<<gdm,b16>>>(m->af_dec_e,m->dWv,m->af_V,S,D,D);
    CUDACHECK(cudaDeviceSynchronize());

    /* Attn scores = Q*K^T/sqrt(D) with causal mask */
    /* Use GPU matmul for Q*K^T then scale+mask on host */
    k_matmul<<<dim3((S+15)/16,(S+15)/16),b16>>>(m->af_Q,m->af_K,m->af_asc,S,S,D);
    CUDACHECK(cudaDeviceSynchronize());
    /* Scale + causal mask on host */
    {
        size_t ss2=SQ*SQ*sizeof(float);
        float*sH=(float*)malloc(ss2);
        CUDACHECK(cudaMemcpy(sH,m->af_asc,ss2,cudaMemcpyDeviceToHost));
        float sc=1.f/sqrtf((float)D);
        for(int i=0;i<S*SQ;i++) sH[i]*=sc;
        for(int i=0;i<S;i++)for(int j=i+1;j<S;j++) sH[i*SQ+j]=-1e9f;
        CUDACHECK(cudaMemcpy(m->af_asc,sH,ss2,cudaMemcpyHostToDevice));free(sH);
    }
    k_softmax<<<1,256>>>(m->af_asc,m->af_apc,S,S);
    k_matmul<<<gdm,b16>>>(m->af_apc,m->af_V,m->af_ao,S,D,S);
    k_matmul<<<gdm,b16>>>(m->af_ao,m->dWo,m->af_r1,S,D,D);
    CUDACHECK(cudaDeviceSynchronize());

    /* Residual + LN1 */
    k_add<<<nemb,256>>>(m->af_dec_e,m->af_r1,m->af_r1,S*D);
    CUDACHECK(cudaDeviceSynchronize());
    k_layernorm<<<1,256>>>(m->af_r1,m->af_ln1,m->dln1g,m->dln1b,S,D);
    CUDACHECK(cudaDeviceSynchronize());

    /* FFN */
    k_matmul<<<gdf,b16>>>(m->af_ln1,m->dW1,m->af_f1,S,F,D);
    k_gelu<<<(S*F+255)/256,256>>>(m->af_f1,m->af_f1g,S*F);
    k_matmul<<<gdm,b16>>>(m->af_f1g,m->dW2,m->af_f2,S,D,F);
    CUDACHECK(cudaDeviceSynchronize());

    /* Residual + LN2 */
    k_add<<<nemb,256>>>(m->af_ln1,m->af_f2,m->af_r2,S*D);
    CUDACHECK(cudaDeviceSynchronize());
    k_layernorm<<<1,256>>>(m->af_r2,m->af_ln2,m->dln2g,m->dln2b,S,D);
    CUDACHECK(cudaDeviceSynchronize());

    /* Logits + softmax */
    k_matmul<<<dim3((voc+15)/16,(S+15)/16),b16>>>(m->af_ln2,m->dWout,m->af_logits,S,voc,D);
    k_softmax<<<1,256>>>(m->af_logits,m->af_probs,S,voc);
    CUDACHECK(cudaDeviceSynchronize());

    /* Loss */
    float*hP=(float*)malloc(SQ*voc*sizeof(float));
    CUDACHECK(cudaMemcpy(hP,m->af_probs,SQ*voc*sizeof(float),cudaMemcpyDeviceToHost));
    float loss=0; int valid=0, nan_count=0;
    for(int t=0;t<S*voc;t++) if(hP[t]!=hP[t]) nan_count++;
    for(int t=0;t<S;t++){int tg=tgt[t];if(tg>0&&tg<voc){float p=hP[t*voc+tg];if(p>1e-6f&&p<=1.f&&!isnan(p)){loss-=logf(p);valid++;}}}    if(valid>0)loss/=valid; else loss=10.f;
    if(step<20||step%50==0){
        snprintf(g_b,sizeof(g_b),"  [step%d] loss=%.4f valid=%d nans=%d\n",step,loss,valid,nan_count);
        log_msg(g_b);
        /* Check for NaN in weights after Adam */
        if(nan_count>0){
            /* Print first few probs to diagnose */
            snprintf(g_b,sizeof(g_b),"    probs[0..3]=%.6e %.6e %.6e %.6e\n",hP[0],hP[1],hP[2],hP[3]);
            log_msg(g_b);
        }
    }
    free(hP);

    /* ====== BACKWARD ====== */
    /* Zero gradient accumulation buffers that use += in k_gA */
    CUDACHECK(cudaMemset(m->ag_dl2,0,S*D*sizeof(float)));
    CUDACHECK(cudaMemset(m->ag_dr1,0,S*D*sizeof(float)));
    CUDACHECK(cudaMemset(m->ag_dao,0,S*D*sizeof(float)));
    CUDACHECK(cudaMemset(m->ag_df1g,0,S*F*sizeof(float)));

    /* d_logits = probs - one_hot(target) */
    k_ce_grad<<<nemb,256>>>(m->af_probs,m->ag_dL,m->gd_tgt,S,voc);
    CUDACHECK(cudaDeviceSynchronize());

    /* gWout += ln2^T * dL */
    k_gB<<<dim3((voc+15)/16,(D+15)/16),b16>>>(m->af_ln2,m->ag_dL,m->ag_Wo2,S,voc,D);
    /* dl2 = dL * Wout^T */
    k_gA<<<gdm,b16>>>(m->ag_dL,m->dWout,m->ag_dl2,S,voc,D);
    CUDACHECK(cudaDeviceSynchronize());

    /* LN2 backward */
    k_ln_g<<<1,256>>>(m->af_r2,m->ag_dl2,m->dln2g,m->ag_dl2,m->ag_ln2g,m->ag_ln2b,S,D);
    CUDACHECK(cudaDeviceSynchronize());

    /* dr2 = dl2 (residual) */
    CUDACHECK(cudaMemcpy(m->ag_dr2,m->ag_dl2,S*D*sizeof(float),cudaMemcpyDeviceToDevice));

    /* FFN backward: W2, GELU, W1 */
    k_gA<<<gdm,b16>>>(m->ag_dr2,m->dW2,m->ag_df1g,S,D,F);
    k_gB<<<gdf,b16>>>(m->af_f1g,m->ag_dr2,m->ag_W2,S,D,F);
    k_gelu_g<<<(S*F+255)/256,256>>>(m->af_f1,m->ag_df1g,m->ag_df1,S*F);
    k_gA<<<gdf,b16>>>(m->ag_df1,m->dW1,m->ag_dr1,S,F,D);
    k_gB<<<gdf,b16>>>(m->af_ln1,m->ag_df1,m->ag_W1,S,F,D);
    CUDACHECK(cudaDeviceSynchronize());

    /* dr1 += dl2 (residual) */
    k_add<<<nemb,256>>>(m->ag_dr1,m->ag_dr2,m->ag_dr1,S*D);
    CUDACHECK(cudaDeviceSynchronize());

    /* LN1 backward */
    k_ln_g<<<1,256>>>(m->af_r1,m->ag_dr1,m->dln1g,m->ag_dl1,m->ag_ln1g,m->ag_ln1b,S,D);
    CUDACHECK(cudaDeviceSynchronize());

    /* Attn backward: Wo, then proper attention backward */
    k_gA<<<gdm,b16>>>(m->ag_dl1,m->dWo,m->ag_dao,S,D,D);
    k_gB<<<gdm,b16>>>(m->af_ao,m->ag_dl1,m->ag_Wo,S,D,D);
    CUDACHECK(cudaDeviceSynchronize());

    /* Proper attention backward on CPU (S=32 is small) */
    {
        float*dao_h=(float*)malloc(S*D*sizeof(float));
        float*V_h=(float*)malloc(S*D*sizeof(float));
        float*apc_h=(float*)malloc(S*S*sizeof(float));
        float*Q_h=(float*)malloc(S*D*sizeof(float));
        float*K_h=(float*)malloc(S*D*sizeof(float));
        CUDACHECK(cudaMemcpy(dao_h,m->ag_dao,S*D*sizeof(float),cudaMemcpyDeviceToHost));
        CUDACHECK(cudaMemcpy(V_h,m->af_V,S*D*sizeof(float),cudaMemcpyDeviceToHost));
        CUDACHECK(cudaMemcpy(apc_h,m->af_apc,S*S*sizeof(float),cudaMemcpyDeviceToHost));
        CUDACHECK(cudaMemcpy(Q_h,m->af_Q,S*D*sizeof(float),cudaMemcpyDeviceToHost));
        CUDACHECK(cudaMemcpy(K_h,m->af_K,S*D*sizeof(float),cudaMemcpyDeviceToHost));
        /* d_attn_probs = dao * V^T [S x S] */
        float*dap_h=(float*)calloc(S*S,sizeof(float));
        for(int i=0;i<S;i++)for(int j=0;j<S;j++){float s=0;for(int d=0;d<D;d++)s+=dao_h[i*D+d]*V_h[j*D+d];dap_h[i*S+j]=s;}
        /* d_V = attn_probs^T * dao [S x D] */
        float*dV_h=(float*)calloc(S*D,sizeof(float));
        for(int j=0;j<S;j++)for(int d=0;d<D;d++){float s=0;for(int i=0;i<S;i++)s+=apc_h[i*S+j]*dao_h[i*D+d];dV_h[j*D+d]=s;}
        /* d_attn_scores = softmax_backward(dap, attn_probs) [S x S] */
        float*ds_h=(float*)malloc(S*S*sizeof(float));
        for(int i=0;i<S;i++){float dot=0;for(int j=0;j<S;j++)dot+=dap_h[i*S+j]*apc_h[i*S+j];for(int j=0;j<S;j++)ds_h[i*S+j]=apc_h[i*S+j]*(dap_h[i*S+j]-dot);/* causal mask: if j>i, ds=0 */for(int j=i+1;j<S;j++)ds_h[i*S+j]=0;}
        /* d_Q = ds * K / sqrt(D) [S x D] */
        float scale=1.f/sqrtf((float)D);
        float*dQ_h=(float*)calloc(S*D,sizeof(float));
        for(int i=0;i<S;i++)for(int d=0;d<D;d++){float s=0;for(int j=0;j<S;j++)s+=ds_h[i*S+j]*K_h[j*D+d];dQ_h[i*D+d]=s*scale;}
        /* d_K = ds^T * Q / sqrt(D) [S x D] */
        float*dK_h=(float*)calloc(S*D,sizeof(float));
        for(int j=0;j<S;j++)for(int d=0;d<D;d++){float s=0;for(int i=0;i<S;i++)s+=ds_h[i*S+j]*Q_h[i*D+d];dK_h[j*D+d]=s*scale;}
        /* Copy back to GPU */
        CUDACHECK(cudaMemcpy(m->ag_dQ,dQ_h,S*D*sizeof(float),cudaMemcpyHostToDevice));
        CUDACHECK(cudaMemcpy(m->ag_dK,dK_h,S*D*sizeof(float),cudaMemcpyHostToDevice));
        CUDACHECK(cudaMemcpy(m->ag_dV,dV_h,S*D*sizeof(float),cudaMemcpyHostToDevice));
        free(dao_h);free(V_h);free(apc_h);free(Q_h);free(K_h);free(dap_h);free(dV_h);free(ds_h);free(dQ_h);free(dK_h);
    }
    CUDACHECK(cudaDeviceSynchronize());

    /* Q,K,V projection grads */
    k_gB<<<gdm,b16>>>(m->af_dec_e,m->ag_dQ,m->ag_Wq,S,D,D);
    k_gB<<<gdm,b16>>>(m->af_dec_e,m->ag_dK,m->ag_Wk,S,D,D);
    k_gB<<<gdm,b16>>>(m->af_dec_e,m->ag_dV,m->ag_Wv,S,D,D);
    /* d_dec_e = dQ*Wq^T + dK*Wk^T + dV*Wv^T + dl1 (residual) (k_gA accumulates with +=) */
    CUDACHECK(cudaMemset(m->ag_dde,0,S*D*sizeof(float)));
    k_gA<<<gdm,b16>>>(m->ag_dQ,m->dWq,m->ag_dde,S,D,D);
    k_gA<<<gdm,b16>>>(m->ag_dK,m->dWk,m->ag_dde,S,D,D);
    k_gA<<<gdm,b16>>>(m->ag_dV,m->dWv,m->ag_dde,S,D,D);
    k_add<<<nemb,256>>>(m->ag_dde,m->ag_dl1,m->ag_dde,S*D);
    CUDACHECK(cudaDeviceSynchronize());

    /* Scatter embed grad */
    k_eg<<<nemb,256>>>(m->ag_emb,m->ag_dde,m->gd_dec,S,D,voc);
    CUDACHECK(cudaDeviceSynchronize());

    /* ====== GRADIENT CLIPPING (global norm) ====== */
    {
        float max_norm=10.0f;
        CUDACHECK(cudaMemset(m->d_normbuf,0,sizeof(float)));
        int nW=DM*DM, nF1=DM*DF, nF2=DF*DM, nv2=voc*DM;
        k_sum_sq<<<(nW+255)/256,256>>>(m->ag_Wq,m->d_normbuf,nW);
        k_sum_sq<<<(nW+255)/256,256>>>(m->ag_Wk,m->d_normbuf,nW);
        k_sum_sq<<<(nW+255)/256,256>>>(m->ag_Wv,m->d_normbuf,nW);
        k_sum_sq<<<(nW+255)/256,256>>>(m->ag_Wo,m->d_normbuf,nW);
        k_sum_sq<<<(nF1+255)/256,256>>>(m->ag_W1,m->d_normbuf,nF1);
        k_sum_sq<<<(nF2+255)/256,256>>>(m->ag_W2,m->d_normbuf,nF2);
        k_sum_sq<<<(nv2+255)/256,256>>>(m->ag_Wo2,m->d_normbuf,nv2);
        k_sum_sq<<<(DM+255)/256,256>>>(m->ag_ln1g,m->d_normbuf,DM);
        k_sum_sq<<<(DM+255)/256,256>>>(m->ag_ln1b,m->d_normbuf,DM);
        k_sum_sq<<<(DM+255)/256,256>>>(m->ag_ln2g,m->d_normbuf,DM);
        k_sum_sq<<<(DM+255)/256,256>>>(m->ag_ln2b,m->d_normbuf,DM);
        k_sum_sq<<<(nv2+255)/256,256>>>(m->ag_emb,m->d_normbuf,nv2);
        CUDACHECK(cudaDeviceSynchronize());
        float h_norm;CUDACHECK(cudaMemcpy(&h_norm,m->d_normbuf,sizeof(float),cudaMemcpyDeviceToHost));
        float gnorm=sqrtf(h_norm);
        if(gnorm>max_norm&&gnorm>0.f){
            float scale=max_norm/gnorm;
            k_scale_all<<<(nW+255)/256,256>>>(m->ag_Wq,scale,nW);
            k_scale_all<<<(nW+255)/256,256>>>(m->ag_Wk,scale,nW);
            k_scale_all<<<(nW+255)/256,256>>>(m->ag_Wv,scale,nW);
            k_scale_all<<<(nW+255)/256,256>>>(m->ag_Wo,scale,nW);
            k_scale_all<<<(nF1+255)/256,256>>>(m->ag_W1,scale,nF1);
            k_scale_all<<<(nF2+255)/256,256>>>(m->ag_W2,scale,nF2);
            k_scale_all<<<(nv2+255)/256,256>>>(m->ag_Wo2,scale,nv2);
            k_scale_all<<<(DM+255)/256,256>>>(m->ag_ln1g,scale,DM);
            k_scale_all<<<(DM+255)/256,256>>>(m->ag_ln1b,scale,DM);
            k_scale_all<<<(DM+255)/256,256>>>(m->ag_ln2g,scale,DM);
            k_scale_all<<<(DM+255)/256,256>>>(m->ag_ln2b,scale,DM);
            k_scale_all<<<(nv2+255)/256,256>>>(m->ag_emb,scale,nv2);
            CUDACHECK(cudaDeviceSynchronize());
        }
    }

    /* ====== ADAM UPDATE ====== */
    int sf=step+1;
    float bt1=1.f-powf(B1,(float)sf),bt2=1.f-powf(B2,(float)sf);
    int nd=DM*DM,nf=DM*DF,nv=voc*DM;
    float wd=0.f;
    k_adam_wd<<<(nd+255)/256,256>>>(m->dWq,m->ag_Wq,m->mWq,m->vWq,LR,B1,B2,EPS,bt1,bt2,wd,nd);
    k_adam_wd<<<(nd+255)/256,256>>>(m->dWk,m->ag_Wk,m->mWk,m->vWk,LR,B1,B2,EPS,bt1,bt2,wd,nd);
    k_adam_wd<<<(nd+255)/256,256>>>(m->dWv,m->ag_Wv,m->mWv,m->vWv,LR,B1,B2,EPS,bt1,bt2,wd,nd);
    k_adam_wd<<<(nd+255)/256,256>>>(m->dWo,m->ag_Wo,m->mWo,m->vWo,LR,B1,B2,EPS,bt1,bt2,wd,nd);
    k_adam_wd<<<(nf+255)/256,256>>>(m->dW1,m->ag_W1,m->mW1,m->vW1,LR,B1,B2,EPS,bt1,bt2,wd,nf);
    k_adam_wd<<<(nd+255)/256,256>>>(m->dW2,m->ag_W2,m->mW2,m->vW2,LR,B1,B2,EPS,bt1,bt2,wd,nd);
    k_adam_wd<<<(nv+255)/256,256>>>(m->dWout,m->ag_Wo2,m->mWo2,m->vWo2,LR,B1,B2,EPS,bt1,bt2,wd,nv);
    k_adam_wd<<<(DM+255)/256,256>>>(m->dln1g,m->ag_ln1g,m->mln1g,m->vln1g,LR,B1,B2,EPS,bt1,bt2,0.f,DM);
    k_adam_wd<<<(DM+255)/256,256>>>(m->dln1b,m->ag_ln1b,m->mln1b,m->vln1b,LR,B1,B2,EPS,bt1,bt2,0.f,DM);
    k_adam_wd<<<(DM+255)/256,256>>>(m->dln2g,m->ag_ln2g,m->mln2g,m->vln2g,LR,B1,B2,EPS,bt1,bt2,0.f,DM);
    k_adam_wd<<<(DM+255)/256,256>>>(m->dln2b,m->ag_ln2b,m->mln2b,m->vln2b,LR,B1,B2,EPS,bt1,bt2,0.f,DM);
    k_adam_wd<<<(nv+255)/256,256>>>(m->d_emb,m->ag_emb,m->memb,m->vemb,LR,B1,B2,EPS,bt1,bt2,wd,nv);
    CUDACHECK(cudaDeviceSynchronize());
    return loss;
}

void model_free(Model*m){
    cudaFree(m->d_emb);cudaFree(m->d_pe);
    cudaFree(m->dWq);cudaFree(m->dWk);cudaFree(m->dWv);cudaFree(m->dWo);
    cudaFree(m->dW1);cudaFree(m->dW2);cudaFree(m->dWout);
    cudaFree(m->dln1g);cudaFree(m->dln1b);cudaFree(m->dln2g);cudaFree(m->dln2b);
    cudaFree(m->mWq);cudaFree(m->vWq);cudaFree(m->mWk);cudaFree(m->vWk);
    cudaFree(m->mWv);cudaFree(m->vWv);cudaFree(m->mWo);cudaFree(m->vWo);
    cudaFree(m->mW1);cudaFree(m->vW1);cudaFree(m->mW2);cudaFree(m->vW2);
    cudaFree(m->mWo2);cudaFree(m->vWo2);
    cudaFree(m->mln1g);cudaFree(m->vln1g);cudaFree(m->mln1b);cudaFree(m->vln1b);
    cudaFree(m->mln2g);cudaFree(m->vln2g);cudaFree(m->mln2b);cudaFree(m->vln2b);
    cudaFree(m->memb);cudaFree(m->vemb);
    cudaFree(m->af_enc_e);cudaFree(m->af_dec_e);
    cudaFree(m->af_Q);cudaFree(m->af_K);cudaFree(m->af_V);
    cudaFree(m->af_asc);cudaFree(m->af_apc);
    cudaFree(m->af_ao);cudaFree(m->af_r1);cudaFree(m->af_ln1);
    cudaFree(m->af_f1);cudaFree(m->af_f1g);cudaFree(m->af_f2);
    cudaFree(m->af_r2);cudaFree(m->af_ln2);
    cudaFree(m->af_logits);cudaFree(m->af_probs);
    cudaFree(m->ag_dL);cudaFree(m->ag_dl2);cudaFree(m->ag_dr2);cudaFree(m->ag_df2);
    cudaFree(m->ag_df1g);cudaFree(m->ag_df1);cudaFree(m->ag_dr1);cudaFree(m->ag_dl1);cudaFree(m->ag_dao);
    cudaFree(m->ag_dQ);cudaFree(m->ag_dK);cudaFree(m->ag_dV);cudaFree(m->ag_dde);
    cudaFree(m->ag_Wq);cudaFree(m->ag_Wk);cudaFree(m->ag_Wv);cudaFree(m->ag_Wo);
    cudaFree(m->ag_W1);cudaFree(m->ag_W2);cudaFree(m->ag_Wo2);
    cudaFree(m->ag_ln1g);cudaFree(m->ag_ln1b);cudaFree(m->ag_ln2g);cudaFree(m->ag_ln2b);cudaFree(m->ag_emb);
    cudaFree(m->gd_enc);cudaFree(m->gd_dec);cudaFree(m->gd_tgt);
    free(m);
}

/* ============ SAVE / LOAD MODEL ============ */
void save_model(Model*m,const char*path,int voc){
    FILE*f=fopen(path,"wb");if(!f){printf("Cannot save %s\n",path);return;}
    fwrite(&voc,sizeof(int),1,f);
    #define SAVEW(p,sz) {float*h=(float*)malloc(sz);CUDACHECK(cudaMemcpy(h,p,sz,cudaMemcpyDeviceToHost));fwrite(h,1,sz,f);free(h);}
    size_t sq=DM*DM*sizeof(float),s1=DM*DF*sizeof(float),s2=DF*DM*sizeof(float),so=voc*DM*sizeof(float),sl=DM*sizeof(float),se=voc*DM*sizeof(float),sp=SQ*DM*sizeof(float);
    SAVEW(m->d_emb,se); SAVEW(m->d_pe,sp);
    SAVEW(m->dWq,sq); SAVEW(m->dWk,sq); SAVEW(m->dWv,sq); SAVEW(m->dWo,sq);
    SAVEW(m->dW1,s1); SAVEW(m->dW2,s2); SAVEW(m->dWout,so);
    SAVEW(m->dln1g,sl); SAVEW(m->dln1b,sl); SAVEW(m->dln2g,sl); SAVEW(m->dln2b,sl);
    fclose(f);
    char b[256];snprintf(b,sizeof(b),"  Model saved: %s (%d vocab)\n",path,voc);log_msg(b);
}
Model* load_model(const char*path,int* voc_out){
    FILE*f=fopen(path,"rb");if(!f)return NULL;
    int voc;fread(&voc,sizeof(int),1,f);
    Model*m=model_create(voc);
    #define LOADW(p,sz) {float*h=(float*)malloc(sz);fread(h,1,sz,f);CUDACHECK(cudaMemcpy(p,h,sz,cudaMemcpyHostToDevice));free(h);}
    size_t sq=DM*DM*sizeof(float),s1=DM*DF*sizeof(float),s2=DF*DM*sizeof(float),so=voc*DM*sizeof(float),sl=DM*sizeof(float),se=voc*DM*sizeof(float),sp=SQ*DM*sizeof(float);
    LOADW(m->d_emb,se); LOADW(m->d_pe,sp);
    LOADW(m->dWq,sq); LOADW(m->dWk,sq); LOADW(m->dWv,sq); LOADW(m->dWo,sq);
    LOADW(m->dW1,s1); LOADW(m->dW2,s2); LOADW(m->dWout,so);
    LOADW(m->dln1g,sl); LOADW(m->dln1b,sl); LOADW(m->dln2g,sl); LOADW(m->dln2b,sl);
    fclose(f);
    *voc_out=voc;
    char b[256];snprintf(b,sizeof(b),"  Model loaded: %s (%d vocab)\n",path,voc);log_msg(b);
    return m;
}

/* ============ GENERATION ============ */
/* Forward-only pass: runs full decoder forward, returns probs at last position */
static void forward_only(Model*m,int voc){
    int S=SQ,D=DM,F=DF;
    dim3 b16(16,16),gdm((D+15)/16,(S+15)/16),gdf((F+15)/16,(S+15)/16);
    int nemb=(S+255)/256;

    /* Embed decoder tokens */
    k_embed<<<nemb,256>>>(m->d_emb,m->gd_dec,m->af_dec_e,S,D,voc);
    k_addpe<<<gdm,b16>>>(m->af_dec_e,m->d_pe,S,D);
    CUDACHECK(cudaDeviceSynchronize());

    /* Self-attn Q,K,V */
    k_matmul<<<gdm,b16>>>(m->af_dec_e,m->dWq,m->af_Q,S,D,D);
    k_matmul<<<gdm,b16>>>(m->af_dec_e,m->dWk,m->af_K,S,D,D);
    k_matmul<<<gdm,b16>>>(m->af_dec_e,m->dWv,m->af_V,S,D,D);
    CUDACHECK(cudaDeviceSynchronize());

    /* Attn scores = Q*K^T/sqrt(D) with causal mask */
    k_matmul<<<dim3((S+15)/16,(S+15)/16),b16>>>(m->af_Q,m->af_K,m->af_asc,S,S,D);
    CUDACHECK(cudaDeviceSynchronize());
    {
        size_t ss2=SQ*SQ*sizeof(float);
        float*sH=(float*)malloc(ss2);
        CUDACHECK(cudaMemcpy(sH,m->af_asc,ss2,cudaMemcpyDeviceToHost));
        float sc=1.f/sqrtf((float)D);
        for(int i=0;i<S*SQ;i++) sH[i]*=sc;
        for(int i=0;i<S;i++)for(int j=i+1;j<S;j++) sH[i*SQ+j]=-1e9f;
        CUDACHECK(cudaMemcpy(m->af_asc,sH,ss2,cudaMemcpyHostToDevice));free(sH);
    }
    k_softmax<<<1,256>>>(m->af_asc,m->af_apc,S,S);
    k_matmul<<<gdm,b16>>>(m->af_apc,m->af_V,m->af_ao,S,D,S);
    k_matmul<<<gdm,b16>>>(m->af_ao,m->dWo,m->af_r1,S,D,D);
    CUDACHECK(cudaDeviceSynchronize());

    /* Residual + LN1 */
    k_add<<<nemb,256>>>(m->af_dec_e,m->af_r1,m->af_r1,S*D);
    CUDACHECK(cudaDeviceSynchronize());
    k_layernorm<<<1,256>>>(m->af_r1,m->af_ln1,m->dln1g,m->dln1b,S,D);
    CUDACHECK(cudaDeviceSynchronize());

    /* FFN */
    k_matmul<<<gdf,b16>>>(m->af_ln1,m->dW1,m->af_f1,S,F,D);
    k_gelu<<<(S*F+255)/256,256>>>(m->af_f1,m->af_f1g,S*F);
    k_matmul<<<gdm,b16>>>(m->af_f1g,m->dW2,m->af_f2,S,D,F);
    CUDACHECK(cudaDeviceSynchronize());

    /* Residual + LN2 */
    k_add<<<nemb,256>>>(m->af_ln1,m->af_f2,m->af_r2,S*D);
    CUDACHECK(cudaDeviceSynchronize());
    k_layernorm<<<1,256>>>(m->af_r2,m->af_ln2,m->dln2g,m->dln2b,S,D);
    CUDACHECK(cudaDeviceSynchronize());

    /* Logits + softmax */
    k_matmul<<<dim3((voc+15)/16,(S+15)/16),b16>>>(m->af_ln2,m->dWout,m->af_logits,S,voc,D);
    k_softmax<<<1,256>>>(m->af_logits,m->af_probs,S,voc);
    CUDACHECK(cudaDeviceSynchronize());
}

void generate(Model*m,Voc*v,const char*prompt,int max_new){
    int vocn=v->n;
    int enc[SQ],elen=0;
    tokenize((char*)prompt,enc,&elen,v);
    int start=(elen>1)?1:0;
    printf("\n--- Generate: \"%s\" ---\n",prompt);
    printf("Input tokens: ");for(int i=0;i<elen;i++)printf("[%s] ",v->w[enc[i]]);printf("\n");
    printf("Output: %s",prompt);

    /* Autoregressive generation.
       Model was trained with: dec[0]=BOS, dec[1]=tok1, ..., dec[n]=PAD...
       We must place tokens at positions 0,1,2,... and pad the rest. */

    int tokens[SQ];
    for(int i=0;i<SQ;i++) tokens[i]=0; /* PAD */
    int tlen=0;
    tokens[tlen++]=2; /* BOS at position 0 */
    for(int i=start;i<elen&&tlen<SQ;i++)tokens[tlen++]=enc[i];

    for(int step=0;step<max_new&&tlen<SQ;step++){
        /* Copy full SQ buffer to GPU */
        CUDACHECK(cudaMemcpy(m->gd_dec,tokens,SQ*sizeof(int),cudaMemcpyHostToDevice));
        /* Forward pass (full SQ positions with causal mask) */
        forward_only(m,vocn);
        /* Get probs for position tlen-1 (last real token position) */
        float*probs=(float*)malloc(vocn*sizeof(float));
        CUDACHECK(cudaMemcpy(probs,m->af_probs+(tlen-1)*vocn,vocn*sizeof(float),cudaMemcpyDeviceToHost));
        /* Mask PAD(0) and UNK(1), renormalize */
        probs[0]=0.f; probs[1]=0.f;
        float psum=0; for(int i=0;i<vocn;i++) if(probs[i]==probs[i]) psum+=probs[i]; /* skip NaN */
        if(psum>1e-10f) for(int i=0;i<vocn;i++) probs[i]/=psum;
        /* Find top-5 */
        int top[5];float topv[5];
        for(int i=0;i<5;i++){top[i]=-1;topv[i]=-1e30f;}
        for(int i=0;i<vocn;i++){
            if(probs[i]>topv[4]||(probs[i]==topv[4]&&top[4]>=0)){topv[4]=probs[i];top[4]=i;
            for(int k=3;k>=0;k--)if(topv[k+1]>topv[k]||(topv[k+1]==topv[k]&&top[k]<0)){float tv=topv[k];topv[k]=topv[k+1];topv[k+1]=tv;int ti=top[k];top[k]=top[4];top[4]=ti;}}
        }
        /* If top[0] still -1, just find first non-zero */
        if(top[0]<0){for(int i=0;i<vocn;i++){if(probs[i]>0&&i!=0&&i!=1){top[0]=i;topv[0]=probs[i];break;}}}
        if(step==0||step%5==0){
            printf("\n  [step %d] top-5: ",step);
            for(int i=0;i<5&&top[i]>=0;i++)printf("[%d]%.6f:%s ",top[i],topv[i],v->w[top[i]]);
            printf("\n  psum=%.6f nan_count=%d ",psum,0);
            for(int i=0;i<vocn;i++) if(probs[i]!=probs[i]) {printf("NAN at %d ",i);break;}
            printf("\n  ");
        }
        int next=top[0];
        if(next==3)break; /* EOS */
        if(next<=1)next=4; /* skip PAD/UNK, try first real token */
        tokens[tlen++]=next;
        printf("%s ",v->w[next]);
        free(probs);
    }
    printf("\n--- End (generated %d tokens) ---\n\n",tlen-(elen-start)-1);
}

/* ============ MAIN ============ */
int main(int argc,char*argv[]){
    g_log=fopen("train_log.txt","w");
    cudaDeviceProp prop;CUDACHECK(cudaGetDeviceProperties(&prop,0));
    log_msg("============================================================\n");
    log_msg("  TRANSFORMER CUDA TRAINING (backprop + Adam)\n");
    log_msg("============================================================\n\n");
    snprintf(g_b,sizeof(g_b),"GPU: %s | VRAM: %d MB\n",prop.name,(int)(prop.totalGlobalMem/1024/1024));log_msg(g_b);
    const char*file=(argc>1)?argv[1]:"harbour_fwh_v3.jsonl";
    int mx=(argc>2)?atoi(argv[2]):5000;
    int ep=(argc>3)?atoi(argv[3]):20;
    snprintf(g_b,sizeof(g_b),"Dataset: %s | Max: %d | Epochs: %d\n",file,mx,ep);log_msg(g_b);
    snprintf(g_b,sizeof(g_b),"Model: d=%d d_ff=%d seq=%d lr=%.4f\n\n",DM,DF,SQ,LR);log_msg(g_b);

    Voc*voc=voc_new();int ns;Smp*data=load_data(file,voc,&ns,mx);
    snprintf(g_b,sizeof(g_b),"Loaded %d samples, vocab %d tokens\n\n",ns,voc->n);log_msg(g_b);
    if(!ns){log_msg("No samples!\n");fclose(g_log);return 1;}

    srand(time(NULL));Model*model=model_create(voc->n);
    long params=(long)voc->n*DM+DM*DM*4+DM*DF*2+DM*4+(long)DM*voc->n;
    snprintf(g_b,sizeof(g_b),"Parameters: %ld (~%.1f MB)\n\n",params,(double)params*4/1024/1024);log_msg(g_b);

    log_msg("============================================================\n");
    log_msg("TRAINING ON GPU\n");
    log_msg("============================================================\n\n");

    clock_t t0=clock();float bl=1e9;
    for(int e=0;e<ep;e++){
        clock_t es=clock();float tl=0;
        int*idx=(int*)malloc(ns*sizeof(int));
        for(int i=0;i<ns;i++)idx[i]=i;
        for(int i=ns-1;i>0;i--){int j=rand()%(i+1);int t=idx[i];idx[i]=idx[j];idx[j]=t;}
        for(int b=0;b<ns;b++){
            int si=idx[b];int enc[SQ],dec[SQ],tgt[SQ];
            memset(enc,0,sizeof(enc));memset(dec,0,sizeof(dec));memset(tgt,0,sizeof(tgt));
            for(int i=0;i<data[si].len&&i<SQ;i++)enc[i]=data[si].enc[i];
            for(int i=0;i<data[si].dlen&&i<SQ;i++)dec[i]=data[si].dec[i];
            for(int i=0;i<data[si].dlen&&i<SQ;i++)tgt[i]=data[si].tgt[i];
            float loss=train_step(model,e*ns+b,enc,dec,tgt,voc->n);
            tl+=loss;
            if((b+1)%200==0){snprintf(g_b,sizeof(g_b),"\r  Batch %d/%d (%.0f%%) Loss: %.4f",b+1,ns,100.f*(b+1)/ns,tl/(b+1));log_msg(g_b);}
        }
        free(idx);
        float et=(float)(clock()-es)/CLOCKS_PER_SEC;float al=tl/ns;
        if(al<bl)bl=al;
        snprintf(g_b,sizeof(g_b),"\n  Epoch %2d/%d | Loss: %.4f | Best: %.4f | %.1fs\n",e+1,ep,al,bl,et);log_msg(g_b);
    }
    float tt=(float)(clock()-t0)/CLOCKS_PER_SEC;
    log_msg("\n============================================================\n");
    snprintf(g_b,sizeof(g_b),"  Loss: %.4f | Time: %.1fs | %.0f samples/s\n",bl,tt,(double)ns*ep/tt);log_msg(g_b);
    log_msg("============================================================\n");

    /* Save model */
    save_model(model,"model.bin",voc->n);

    /* ========== GENERATION TESTS ========== */
    log_msg("\n============================================================\n");
    log_msg("GENERATION TESTS\n");
    log_msg("============================================================\n");

    /* Test with actual training samples */
    int test_indices[]={0,100,500,1000,2000};
    for(int ti=0;ti<5;ti++){
        int si=test_indices[ti];
        if(si>=ns){printf("  skip %d (ns=%d)\n",si,ns);continue;}
        printf("\n--- Test sample %d ---\n",si);
        int elen=data[si].len;
        int dlen=data[si].dlen;
        printf("Enc tokens (len=%d): ",elen);
        for(int i=0;i<elen;i++) printf("[%s] ",voc->w[data[si].enc[i]]);
        printf("\nTarget tokens (dlen=%d): ",dlen);
        for(int i=0;i<dlen;i++) printf("[%s] ",voc->w[data[si].tgt[i]]);
        printf("\n  dec tokens: ");
        for(int i=0;i<dlen;i++) printf("[%s] ",voc->w[data[si].dec[i]]);
        printf("\n");

        /* Run forward pass on this sample's decoder input */
        int dec_buf[SQ];for(int i=0;i<SQ;i++)dec_buf[i]=0;
        for(int i=0;i<dlen&&i<SQ;i++)dec_buf[i]=data[si].dec[i];
        CUDACHECK(cudaMemcpy(model->gd_dec,dec_buf,SQ*sizeof(int),cudaMemcpyHostToDevice));
        printf("  calling forward_only (dlen=%d)...\n",dlen);fflush(stdout);
        forward_only(model,voc->n);
        printf("  forward_only done, checking probs...\n");fflush(stdout);

        /* Check probs at each valid position */
        float*probs=(float*)malloc(voc->n*sizeof(float));
        int correct=0;
        for(int pos=0;pos<dlen&&pos<SQ;pos++){
            CUDACHECK(cudaMemcpy(probs,model->af_probs+pos*voc->n,voc->n*sizeof(float),cudaMemcpyDeviceToHost));
            int tgt=data[si].tgt[pos];
            float tgt_prob=(tgt>=0&&tgt<voc->n)?probs[tgt]:0.f;
            /* Find argmax (skip PAD=0, UNK=1) */
            int argm=2;float argmv=probs[2];
            for(int i=3;i<voc->n;i++){if(probs[i]>argmv){argmv=probs[i];argm=i;}}
            if(argm==tgt)correct++;
            printf("  pos %2d: target=%-20s(%.6f) argmax=%-20s(%.6f) %s\n",
                pos,voc->w[tgt],tgt_prob,voc->w[argm],argmv,(argm==tgt)?"OK":"");
            fflush(stdout);
        }
        printf("  Summary: %d/%d correct (%.0f%%)\n",correct,dlen,100.f*correct/dlen);
        fflush(stdout);
        free(probs);
    }

    /* Free data before generation tests (we need voc) */

    /* Cleanup */
    for(int i=0;i<ns;i++){free(data[i].enc);free(data[i].dec);free(data[i].tgt);}free(data);
    for(int i=0;i<voc->n;i++)free(voc->w[i]);free(voc->w);free(voc);
    model_free(model);
    log_msg("\nDONE - see train_log.txt\n");
    fclose(g_log);return 0;
}
