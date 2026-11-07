import mido,time,statistics
n=[x for x in mido.get_input_names() if 'FM-1' in x][0]
inp=mido.open_input(n); out=mido.open_output([x for x in mido.get_output_names() if 'FM-1' in x][0])
def cc(c,v): out.send(mido.Message('control_change',control=c,value=v))
def meas(sec=3):
    c=[];t=time.time()
    while time.time()-t<sec:
        for m in inp.iter_pending():
            if m.type=='control_change' and m.control==100:c.append(m.value)
        time.sleep(0.02)
    return max(c) if c else None
cc(110,4)   # four voices
time.sleep(1.5)
for name,mask in [("all",0),("no pitchmod",1),("no osc",2),("no noise",4),("no dist",8),("no eq",16),("osc only path (no noise,dist,eq)",4|8|16),("noise only (no osc,mod)",2|1|8|16),("nothing",31),("nothing, no ctl",31|64),("skeleton only",32)]:
    cc(111,mask); time.sleep(0.8); print("%-34s cpu%% (4 voices): %s"%(name,meas()))
cc(111,0);cc(110,0)
