import math, json
# Exact replica of the TQ gate in src/chat.c (lines ~55-270), two modes:
# pre  = byte-wise C-locale isalnum (current code)
# post = FoldChar-based consumption (proposed fix)
UTF8={0x81:'a',0xA1:'a',0x89:'e',0xA9:'e',0x8D:'i',0xAD:'i',0x91:'n',0xB1:'n',0x93:'o',0xB3:'o',0x9A:'u',0xBA:'u',0x9C:'u',0xBC:'u'}
LAT={0xE1:'a',0xC1:'a',0xE9:'e',0xC9:'e',0xED:'i',0xCD:'i',0xF3:'o',0xD3:'o',0xFA:'u',0xDA:'u',0xFC:'u',0xDC:'u',0xF1:'n',0xD1:'n'}
CP850={0x82:'e',0x90:'e',0xA0:'a',0xA1:'i',0xA2:'o',0xA3:'u',0x81:'u',0x9A:'u',0xA4:'n',0xA5:'n'}
def fold_bytes(bs):
    out=bytearray();i=0
    while i<len(bs):
        c=bs[i]
        if c==0xC3 and i+1<len(bs) and bs[i+1]>=0x80:
            out.append(ord(UTF8.get(bs[i+1],'?')));i+=2;continue
        if c in LAT:out.append(ord(LAT[c]))
        elif c in CP850:out.append(ord(CP850[c]))
        elif c>=0x80:out.append(ord('?'))
        else:out.append(ord(chr(c).lower()))
        i+=1
    return bytes(out)
def isalnum_b(c):return (48<=c<=57)or(65<=c<=90)or(97<=c<=122)
def tq_words(sent,mode):
    # returns list of unique words >=3 bytes, <=23 bytes
    words=[];cur=bytearray()
    if mode=='pre':
        for c in sent:
            if isalnum_b(c):cur.append(ord(chr(c).lower()))
            else:cur.append(0)
    else:
        i=0
        while i<len(sent):
            if sent[i]==0:cur.append(0);i+=1;continue
            c=sent[i]
            if c==0xC3 and i+1<len(sent) and sent[i+1]>=0x80:
                f=UTF8.get(sent[i+1],'?');i+=2
            elif c in LAT:f=LAT[c];i+=1
            elif c in CP850:f=CP850[c];i+=1
            elif c>=0x80:f='?';i+=1
            else:f=chr(c).lower();i+=1
            fb=ord(f)
            if isalnum_b(fb):cur.append(fb)
            else:cur.append(0)
    for w in bytes(cur).split(b'\0'):
        if 3<=len(w)<=23 or (len(w)>23 and (w:=w[:23])):
            if len(w)>=3 and w not in words:words.append(w)
    return [w for w in words if len(w)>=3]
def build_vocab(sents,mode):
    df={}
    for s in sents:
        for w in tq_words(s,mode):df[w]=df.get(w,0)+1
    return df
def known(w,df):
    if w in df:return True
    n=len(w)
    for v in df:
        m=len(v)
        if m>=4 and v[:min(m,n)]==w[:min(m,n)] and min(m,n)>=4 and abs(n-m)<=2:return True
    return False
def idf(w,df,nsent):return math.log((nsent+1)/(df.get(w,0)+0.5))
def coverage(words,sent,df,nsent,mode):
    low=sent.lower() if mode=='pre' else fold_bytes(sent)
    hit=all_=0.0
    for w in words:
        ww=w.lower() if mode=='pre' else fold_bytes(w)
        i=idf(ww,df,nsent);all_+=i
        start=0
        while True:
            h=low.find(ww,start)
            if h<0:break
            if h==0 or not isalnum_b(low[h-1]):hit+=i;break
            start=h+1
    return hit/all_ if all_>0 else 1.0
STOP=set('de of del en in on at for with y e and o u or por el la los las the un una unos unas a an que quien quienes cual cuales donde como cuantos cuantas who what where when why how which whom whose su sus his her its their mi my tu your hay there hubo hubieron es era fue son is was are were be been do did does no not si yes'.split())
def qwords(q):
    toks=[t.replace('?','') for t in fold_bytes(q.encode()).decode().split()]
    return [w.encode() for w in toks if w not in STOP and len(w)>=3]
def gate(words,df,nsent,mode):
    kw=[w for w in words if known(w,df)]
    nk=len(kw);ncount=len(words)
    if nk==0 or 2*nk<ncount:return None,kw
    return kw,kw
def run(corpus_sents,query,mode,shown=frozenset()):
    df=build_vocab(corpus_sents,mode);words=qwords(query)
    kw=[w for w in words if known(w,df)]
    if not kw or 2*len(kw)<len(words):return ('ABSTAIN-nk',None,0.0,kw)
    best=None;bc=-1;cs={}
    for i,s in enumerate(corpus_sents):
        if i in shown:continue
        c=coverage(kw,s,df,len(corpus_sents),mode);cs[i]=round(c,4)
        if c>0.5 and c>bc:bc=c;best=i
    if best is None:return ('ABSTAIN-gate',None,cs,kw)
    return ('ANSWER',best,cs,kw)
def load(p):return [l.strip().encode() for l in open(p) if l.strip()]
if __name__=='__main__':
    import sys
    base='/tmp/symbols-clone/tools/chat_coverage/fixtures/'
    # validate pre-fix against observed debug
    m=load(base+'memory.txt');a=load(base+'astronomy.txt');mm=load(base+'math.txt')
    print('VALIDATE pre:')
    r=run(a,'¿Qué emite la colisión durante el destello?','pre');print('astro1 exact-candidate cs:',r)
    r=run(m,'¿Qué conserva el hipocampo durante el sueño?','pre');print('mem1:',r)
    r=run(mm,'¿Qué devuelve la función durante la evaluación?','pre');print('math1:',r)
