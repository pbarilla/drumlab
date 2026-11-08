import re,sys
def parse(path):
    txt=open(path,encoding='utf-8',errors='replace').read()
    pre={}
    m=re.search(r'DrumPatches: \{(.*?)\n\tPatterns: ',txt,re.S)
    drums=[]
    for b in re.finditer(r'\n\t\t(\d): \{(.*?)\n\t\t\}',m.group(1),re.S):
        d=dict(re.findall(r'(\w+) = (.*)',b.group(2)))
        drums.append(d)
    return drums
def mtdrum(d):
    mode=d['ModMode']
    def f(k): return d[k].strip()
    def num(k):
        v=f(k); return '-inf' if v=='-inf' else v
    L=[]
    L.append('OscWave: %s'%f('OscWave'))
    L.append('OscFreq: %s Hz'%f('OscFreq'))
    L.append('OscAtk: %s ms'%f('OscAtk'))
    L.append('OscDcy: %s ms'%f('OscDcy'))
    L.append('ModMode: %s'%{'Decay':'Decay','Sine':'Sine','Noise':'Noise FM'}[mode])
    L.append('ModRate: %s %s'%(f('ModRate'),'ms' if mode=='Decay' else 'Hz'))
    L.append('ModAmt: %s st'%f('ModAmt'))
    L.append('NFilMod: %s'%{'LP':'LP (low-pass)','BP':'BP (band-pass)','HP':'HP (hi-pass)'}[f('NFilMod')])
    L.append('NFilFrq: %s Hz'%f('NFilFrq'))
    L.append('NFilQ: %s'%f('NFilQ'))
    L.append('NEnvMod: %s'%{'Exp':'Exponential','Linear':'Linear','Mod':'Modulate'}[f('NEnvMod')])
    L.append('NEnvAtk: %s ms'%f('NEnvAtk'))
    L.append('NEnvDcy: %s ms'%f('NEnvDcy'))
    mix=float(f('Mix'))
    L.append('Mix: %.6f%% osc, %.6f%% noise'%(mix,100-mix))      # Mix in a .mtpreset is the OSCILLATOR's share (found 2026-11-09: BD 86, hats 33 on average); this had it the other way round
    L.append('DistAmt: %s%%'%f('DistAmt'))
    L.append('EQFreq: %s Hz'%f('EQFreq'))
    L.append('EQGain: %s dB'%f('EQGain'))
    L.append('Level: %s dB'%f('Level'))
    for k in('OscVel','NVel','ModVel'): L.append('%s: %s%%'%(k,f(k)))
    return '\n'.join(L)+'\n'
if __name__=='__main__':
    ds=parse(sys.argv[1])
    for i,d in enumerate(ds): open('%s/d%d.txt'%(sys.argv[2],i),'w').write(mtdrum(d))
    print(len(ds),set(d['OscWave'] for d in ds),set(d['NEnvMod'] for d in ds),set(d['ModMode'] for d in ds))
