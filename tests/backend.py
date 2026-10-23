#!/usr/bin/python3
# SPDX-License-Identifier: Apache-2.0
import os,subprocess,sys,tempfile
with tempfile.TemporaryDirectory() as tmp:
 env={**os.environ,'FLTK_BACKEND':'x11','XDG_STATE_HOME':tmp}
 result=subprocess.run([sys.argv[1],'--demo','--ui-capture',tmp+'/forbidden.ppm'],env=env,capture_output=True,text=True,timeout=8)
 assert result.returncode==1 and 'build-time opt-in' in result.stderr
 env['FLTK_BACKEND']='wayland';env['WAYLAND_DISPLAY']='openbrush-no-such-wayland-socket'
 result=subprocess.run([sys.argv[1],'--demo','--ui-capture',tmp+'/forbidden.ppm'],env=env,capture_output=True,text=True,timeout=8)
 assert result.returncode!=0,'Default build unexpectedly fell back to X11'
 print('Default frontend rejects X11 and fails cleanly without its Wayland socket')
