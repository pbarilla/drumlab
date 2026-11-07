import mido,time,sys,statistics
names=mido.get_input_names(); print(names)
n=[x for x in names if 'FM-1' in x or 'Felucca' in x][0]
inp=mido.open_input(n); out=mido.open_output([x for x in mido.get_output_names() if 'FM-1' in x or 'Felucca' in x][0])
def window(sec):
    cpu=[];trig=[];t=time.time()
    while time.time()-t<sec:
        for m in inp.iter_pending():
            if m.type=='control_change':
                (cpu if m.control==100 else trig if m.control==101 else []).append(m.value)
        time.sleep(0.02)
    return cpu,trig
for k in [0,1,2,4,8]:
    out.send(mido.Message('control_change',control=110,value=k)); window(1.2)
    c,t=window(4)
    print("voices every 60ms: %d  cpu%% max %s median %s | trigger x10us max %s"%(k,max(c) if c else None,statistics.median(c) if c else None,max(t) if t else None))
out.send(mido.Message('control_change',control=110,value=0))
