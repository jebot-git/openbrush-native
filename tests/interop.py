#!/usr/bin/python3
# SPDX-License-Identifier: Apache-2.0
"""Independent Python reader checks C++ output against the upstream .tilt fixture."""
import json,pathlib,struct,subprocess,sys,tempfile,zipfile

def read(path):
 with zipfile.ZipFile(path) as archive:
  assert archive.testzip() is None
  metadata=json.loads(archive.read('metadata.json'))
  data=archive.read('data.sketch');offset=0
  def unpack(fmt):
   nonlocal offset
   result=struct.unpack_from('<'+fmt,data,offset);offset+=struct.calcsize('<'+fmt)
   return result
  sentinel,version,reserved,header_size=unpack('4I')
  assert sentinel==0xc576a5cd and version in (5,6) and reserved==0
  offset+=header_size
  count,=unpack('I');strokes=[]
  for _ in range(count):
   index,=unpack('I');color=unpack('4f');size,=unpack('f');sm,pm=unpack('2I');extensions={}
   for bit in range(32):
    if sm&(1<<bit):
     n=4 if bit<16 else unpack('I')[0]
     extensions[bit]=data[offset:offset+n];offset+=n
   points=[]
   for _ in range(unpack('I')[0]):
    pose=unpack('7f');fields={}
    for bit in range(32):
     if pm&(1<<bit):fields[bit]=unpack('f' if bit==0 else 'I')[0]
    points.append((pose,fields))
   strokes.append((metadata['BrushIndex'][index],color,size,extensions,points))
  assert offset==len(data)
  return metadata,strokes,{n:archive.read(n) for n in archive.namelist() if n not in ('metadata.json','data.sketch')}

with tempfile.TemporaryDirectory() as tmp:
 output=pathlib.Path(tmp)/'interop.tilt'
 subprocess.run([sys.argv[1],sys.argv[2],str(output)],check=True)
 original=read(sys.argv[2]);saved=read(output)
 assert original[2]==saved[2]
 for key,value in original[0].items():assert saved[0][key]==value
 assert len(original[1])==len(saved[1])
 for a,b in zip(original[1],saved[1]):
  assert a[0:2]==b[0:2]
  assert abs(a[2]-b[2])<1e-5
  for bit,value in a[3].items():assert b[3][bit]==value
  for p,q in zip(a[4],b[4]):
   assert all(abs(x-y)<1e-5 for x,y in zip(p[0],q[0]))
   assert p[1]==q[1]
 print('Independent ZIP and sketch reader: metadata, attachments, brush IDs, colors, units, poses and extension values agree')
