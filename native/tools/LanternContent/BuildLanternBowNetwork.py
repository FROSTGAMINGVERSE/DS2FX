# DS2FX-owned binary patcher for the exact installed Scholar c0001 network.
# Appends one private item-use leaf; all original animation leaves remain intact.
import struct, hashlib, json, sys
from pathlib import Path
src=Path(sys.argv[1]);dst=Path(sys.argv[2]);original=src.read_bytes()
def need(v,msg):
 if not v:raise ValueError(msg)
def u16(b,o):return struct.unpack_from('<H',b,o)[0]
def u32(b,o):return struct.unpack_from('<I',b,o)[0]
def q(b,o):return struct.unpack_from('<Q',b,o)[0]
def put16(b,o,v):struct.pack_into('<H',b,o,v)
def put32(b,o,v):struct.pack_into('<I',b,o,v)
def putq(b,o,v):struct.pack_into('<Q',b,o,v)
def packets(b):
 out=[];p=0
 while p+48<=len(b):
  need(u32(b,p)==24 and u32(b,p+4)==10,'Unknown packet version')
  size=q(b,p+32);align=u32(b,p+40);need(align in(4,8,16),'Unknown alignment')
  d=(p+48+align-1)&~(align-1);need(d+size<=len(b),'Truncated packet');out.append((p,d,u32(b,p+8),size));p=d+size
 need(b[p:]==b'\0'*(len(b)-p) and len(b)-p<16,'Unknown trailer');return out
need(hashlib.sha256(original).hexdigest()=='c2b958e805503f583f8105dc878c51b7e824c846a2bbc9b274e78a0d278951d1','Unknown source network hash')
pk=packets(original);np=[x for x in pk if x[2]==10];need(len(np)==1,'Expected one network');ph,ns,_,size=np[0]
net=bytearray(original[ns:ns+size]);need(u32(net,0x90)==9189 and q(net,0x98)==0x26e4,'Unknown network layout')
nodes=list(struct.unpack_from('<9189Q',net,0x26e4));selector=nodes[5978];powder=nodes[5979];bow=nodes[4506]
need(u32(net,selector)==131 and u16(net,selector+10)==7 and u16(net,powder+6)==5979 and u32(net,powder)==104,'Unexpected item branch')
child=q(net,selector+32);need(struct.unpack_from('<7H',net,selector+child)==(5979,5980,5981,5982,5983,6006,5984),'Item selector children changed')
aha=selector+q(net,selector+48);weights=q(net,aha);need(struct.unpack_from('<7f',net,selector+weights+32)==(20.,30.,40.,50.,60.,80.,90.),'Item selector values changed')
need(u32(net,selector+weights+16)==7 and q(net,selector+weights+24)==32,'Unexpected float array')
need(nodes[5980]-powder==0x4b0 and u16(net,powder+16)==8,'Unknown leaf layout')
leaf=bytearray(net[powder:nodes[5980]])
for h in(1,5):
 pa=q(net,powder+48)+h*24;ba=q(net,bow+48)+h*24
 p=q(net,powder+pa);r=q(net,bow+ba)
 need(q(net,powder+pa+8)==160 and q(net,bow+ba+8)==160,'Unexpected source animation size')
 need(u32(net,powder+p+0x74)==584 and u32(net,bow+r+0x74)==559,'Clip IDs changed')
 # Retain all item-use event tracks. Only the source animation attributes change.
 leaf[p:p+160]=net[bow+r:bow+r+160]
put16(leaf,6,9189)
def append(data,align=16):
 net.extend(b'\0'*((-len(net))%align));at=len(net);net.extend(data);return at
own=append(leaf)
# Existing selector has a spare child slot and one spare float in its allocation.
put16(net,selector+10,8);struct.pack_into('<8H',net,selector+child,9189,5979,5980,5981,5982,5983,6006,5984)
put32(net,selector+weights+16,8);struct.pack_into('<8f',net,selector+weights+32,15.,20.,30.,40.,50.,60.,80.,90.)
# Grow the lookup array without relocating any original node.
nt=append(struct.pack('<9190Q',*(nodes+[own])));put32(net,0x90,9190);putq(net,0x98,nt)
# Extend debug-name lookup; all existing name entries and bytes survive.
nam=q(net,0xc8);cnt=u32(net,nam);dlen=u32(net,nam+4);ip=q(net,nam+8);op=q(net,nam+16);dp=q(net,nam+24)
ids=list(struct.unpack_from('<'+str(cnt)+'I',net,nam+ip));offs=list(struct.unpack_from('<'+str(cnt)+'I',net,nam+op));need(max(ids)<9189,'Private node name already used')
data=bytes(net[nam+dp:nam+dp+dlen]);name=b'DS2FX_Lantern_EmptyBowReach\0';count=cnt+1
names=struct.pack('<IIQQQ',count,dlen+len(name),32,32+count*4,32+count*8)+struct.pack('<'+str(count)+'I',*(ids+[9189]))+struct.pack('<'+str(count)+'I',*(offs+[dlen]))+data+name
new_names=append(names);putq(net,0xc8,new_names)
# Empty original tag table still requires one length entry for every node.
tags=q(net,0xe0);need(u16(net,tags+24)==0 and u16(net,tags+26)==9189 and q(net,tags+8)==0,'Unexpected nonempty tag table')
lengths=bytes(net[tags+q(net,tags+16):tags+q(net,tags+16)+9189*2]);need(lengths==b'\0'*(9189*2),'Unexpected node tags')
new_tags=append(struct.pack('<QQQHHI',32,0,32,0,9190,0xcdcdcdcd)+lengths+b'\0\0');putq(net,0xe0,new_tags)
net.extend(b'\0'*((-len(net))%16))
# Prove every original node except the declared selector is byte-identical.
for i,o in enumerate(nodes):
 end=nodes[i+1] if i+1<len(nodes) else q(original,ns+0xe0)
 if i==0 or i==5978:continue
 need(net[o:end]==original[ns+o:ns+end],'Original node changed: '+str(i))
need(net[powder:nodes[5980]]==original[ns+powder:ns+nodes[5980]],'Repair Powder leaf changed')
# Clone differs only in node identity and the two animation-source attributes.
allowed=set(range(6,8))
for h in(1,5):
 p=q(original,ns+powder+q(original,ns+powder+48)+h*24);allowed.update(range(p,p+160))
for i,(a,b) in enumerate(zip(leaf,original[ns+powder:ns+nodes[5980]])):need(a==b or i in allowed,'Item event changed in clone')
result=bytearray(original[:ns])+net+original[ns+size:];putq(result,ph+32,len(net));rp=packets(result);need(len(rp)==len(pk),'Packet count changed')
for a,c in zip(pk,rp):
 if a[2]!=10:need(original[a[1]:a[1]+a[3]]==result[c[1]:c[1]+c[3]],'Unrelated packet changed')
need(not dst.exists(),'Fresh output required');dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(result)
report={'InputSHA256':hashlib.sha256(original).hexdigest(),'OutputSHA256':hashlib.sha256(result).hexdigest(),'OriginalNodeCount':9189,'NodeCount':9190,'PrivateCategory':15,'PrivateNode':9189,'SourceAnimation':559,'Motion':40215030,'PreservedItemEventSourceNode':5979,'OriginalPowderAndBowLeavesUnchanged':True,'AllNonNetworkPacketsUnchanged':True,'LiveValidated':False}
dst.with_suffix('.audit.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))

