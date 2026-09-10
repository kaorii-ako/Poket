exec(open('/var/home/hxshino/projects/Poket/cad/enclosure.py').read().split('for name, shape in')[0])
for i,s in enumerate(front.Solids):
    print("front solid",i,"vol_cm3=%.3f"%(s.Volume/1000),"bbox",s.BoundBox)
