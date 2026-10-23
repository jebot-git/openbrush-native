#!/usr/bin/python3
# SPDX-License-Identifier: Apache-2.0
import pathlib,subprocess,sys
binary,output=sys.argv[1:]
subprocess.run([binary,'--demo','--render',output],check=True)
with open(output,'rb') as stream:
 assert stream.readline()==b'P6\n'
 assert stream.readline()==b'640 640\n'
 assert stream.readline()==b'255\n'
 pixels=stream.read()
assert len(pixels)==640*640*3
background=pixels[:3]
foreground=[pixels[i:i+3] for i in range(0,len(pixels),3) if pixels[i:i+3]!=background]
assert len(foreground)>5000,'Native render is blank or misses brush geometry'
for channel in range(3):
 assert any(p[channel]>150 and p[channel]>max(p[(channel+1)%3],p[(channel+2)%3])*2 for p in foreground),'Missing colored brush'
print(f'Native OpenGL render: {len(foreground)} colored foreground pixels; all three brush colors visible')
