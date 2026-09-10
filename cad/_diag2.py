src=open('/var/home/hxshino/projects/Poket/cad/enclosure.py').read().split('for name, shape in')[0]
exec(src)
print("front vol cm3 %.3f"%(front.Volume/1000))
print("n cuts_front", len(cuts_front))
for i,c in enumerate(cuts_front):
    b=c.BoundBox
    print("CUT %d x[%.1f %.1f] y[%.1f %.1f] z[%.1f %.1f] vol %.2f"%(i,b.XMin,b.XMax,b.YMin,b.YMax,b.ZMin,b.ZMax,c.Volume))
