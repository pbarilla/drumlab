import sys,struct,numpy as np
d=np.frombuffer(open(sys.argv[1],'rb').read(),dtype='<i2').astype(float)
o=d[0::2]/32768; r=d[1::2]/32768
n=1024; w=np.hanning(n); L=[]; E=[]
for i in range(0,len(o)-n,512):
    a=np.abs(np.fft.rfft(o[i:i+n]*w)); b=np.abs(np.fft.rfft(r[i:i+n]*w))
    if (b**2).sum()<1e-6: continue
    # compare in bands (smooth) to ignore phase-ish fine structure
    ba=np.array([a[k:k+8].mean() for k in range(0,len(a)-8,8)]); bb=np.array([b[k:k+8].mean() for k in range(0,len(b)-8,8)])
    m=bb>bb.max()*1e-3
    L.append(np.mean(np.abs(20*np.log10((ba[m]+1e-9)/(bb[m]+1e-9)))))
    E.append(20*np.log10((np.sqrt((o[i:i+n]**2).mean())+1e-9)/(np.sqrt((r[i:i+n]**2).mean())+1e-9)))
print("spectral dev %.2f dB (mean abs), level dev %.2f dB (mean), frames %d"%(np.mean(L),np.mean(E),len(L)))
