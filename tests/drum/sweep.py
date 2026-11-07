import sys,random,glob,subprocess,os,re
sys.path.insert(0,'.')
import mt2txt, numpy as np
random.seed(int(sys.argv[1]) if len(sys.argv)>1 else 1)
files=glob.glob("/Users/pasqualebarilla/Downloads/Patternarium G01-G25/*/*.mtpreset")
pick=random.sample(files,50)
os.makedirs('sw',exist_ok=True)
rows=[]
for fi,f in enumerate(pick):
    try: ds=mt2txt.parse(f)
    except Exception as e: continue
    for j,d in enumerate(ds):
        open('sw/d.txt','w').write(mt2txt.mtdrum(d))
        vel=random.choice([127,127,100,60])
        r=subprocess.run(['./cmp','sw/d.txt','sw/x.raw',str(vel)],capture_output=True,text=True).stdout
        if 'len' not in r: rows.append((f,j,'ERR '+r)); continue
        raw=np.frombuffer(open('sw/x.raw','rb').read(),dtype='<i2').astype(float)
        o=raw[0::2]/32768; rf=raw[1::2]/32768
        n=1024;w=np.hanning(n);L=[];E=[]
        for i in range(0,len(o)-n,512):
            if (rf[i:i+n]**2).mean()<1e-7: continue
            a=np.abs(np.fft.rfft(o[i:i+n]*w));b=np.abs(np.fft.rfft(rf[i:i+n]*w))
            ba=np.array([a[k:k+8].mean() for k in range(0,len(a)-8,8)]);bb=np.array([b[k:k+8].mean() for k in range(0,len(b)-8,8)])
            m=bb>bb.max()*1e-3
            L.append(np.mean(np.abs(20*np.log10((ba[m]+1e-9)/(bb[m]+1e-9)))))
            E.append(20*np.log10((np.sqrt((o[i:i+n]**2).mean())+1e-9)/(np.sqrt((rf[i:i+n]**2).mean())+1e-9)))
        tail=re.search(r'tail-ref-rms (\S+)',r)
        if L: rows.append((os.path.basename(f),j,vel,np.mean(L),np.mean(E),float(tail.group(1)) if tail else 0))
good=[r for r in rows if len(r)>3]
sp=np.array([r[3] for r in good]);lv=np.array([r[4] for r in good])
print("n",len(good),"errors",len(rows)-len(good))
print("spectral dev: median %.2f  p90 %.2f  max %.2f"%(np.median(sp),np.percentile(sp,90),sp.max()))
print("level dev   : median %.2f  p90 %.2f  max |%.2f|"%(np.median(np.abs(lv)),np.percentile(np.abs(lv),90),np.abs(lv).max()))
bad=[r for r in good if r[3]>2.5 or abs(r[4])>2 or r[5]>-60]
print("outliers",len(bad))
for r in sorted(bad,key=lambda r:-r[3])[:12]: print("  ",r)
