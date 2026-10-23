#!/usr/bin/python3
# SPDX-License-Identifier: Apache-2.0
import os,pathlib,shutil,signal,subprocess,sys,tempfile
signal.pthread_sigmask(signal.SIG_SETMASK,[])
with tempfile.TemporaryDirectory() as tmp:
 tmp=pathlib.Path(tmp);path=tmp/'artwork.tilt';shutil.copyfile(sys.argv[2],path)
 env={**os.environ,'XDG_CURRENT_DESKTOP':'Dome','FLTK_BACKEND':'x11','LIBGL_ALWAYS_SOFTWARE':'1','XDG_CONFIG_HOME':str(tmp/'config'),'XDG_STATE_HOME':str(tmp/'state')}
 env.pop('WAYLAND_DISPLAY',None)
 subprocess.run(['xvfb-run','-a','dbus-run-session','--',sys.argv[1],str(path),sys.argv[3]],env=env,check=True,timeout=35)
