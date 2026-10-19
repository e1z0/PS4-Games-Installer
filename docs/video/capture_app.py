"""Record the actual desktop preview entering/testing/saving the tutorial URL.
This is expressly not PS4 footage. The dedicated data mount isolates preview state.
"""
import ctypes as C
import os
from pathlib import Path
import subprocess
import time
ROOT=Path(__file__).resolve().parents[2];WORK=ROOT/'build/setup-video';FRAMES=WORK/'frames'
X=C.CDLL('libX11.so.6');T=C.CDLL('libXtst.so.6')
X.XOpenDisplay.argtypes=[C.c_char_p];X.XOpenDisplay.restype=C.c_void_p
X.XDefaultRootWindow.argtypes=[C.c_void_p];X.XDefaultRootWindow.restype=C.c_ulong
X.XKeysymToKeycode.argtypes=[C.c_void_p,C.c_ulong];X.XKeysymToKeycode.restype=C.c_uint
X.XQueryTree.argtypes=[C.c_void_p,C.c_ulong,C.POINTER(C.c_ulong),C.POINTER(C.c_ulong),C.POINTER(C.POINTER(C.c_ulong)),C.POINTER(C.c_uint)]
X.XMoveResizeWindow.argtypes=[C.c_void_p,C.c_ulong,C.c_int,C.c_int,C.c_uint,C.c_uint]
X.XSetInputFocus.argtypes=[C.c_void_p,C.c_ulong,C.c_int,C.c_ulong]
X.XFlush.argtypes=[C.c_void_p];X.XFree.argtypes=[C.c_void_p];X.XCloseDisplay.argtypes=[C.c_void_p]
T.XTestFakeKeyEvent.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_ulong]
if Path('/tmp/.X11-unix/X109').exists():
 raise RuntimeError('Display :109 is already in use; choose an unused capture display')
existing=subprocess.run(['docker','inspect','psgi-video-app-preview'],capture_output=True)
if existing.returncode==0:
 raise RuntimeError('Capture container name already exists; leave it untouched and choose another')
data=WORK/'app-preview-data';data.mkdir(exist_ok=True)
log=(WORK/'app-preview.log').open('wb')
xvfb=subprocess.Popen(['Xvfb',':109','-screen','0','1920x1080x24','-nolisten','tcp','-ac'],stdout=log,stderr=log)
container=None;display=None
try:
 time.sleep(1)
 container=subprocess.Popen(['docker','run','--rm','--name','psgi-video-app-preview','--user',f'{os.getuid()}:{os.getgid()}',
  '-v',f'{ROOT}:/work:ro','-v',f'{data}:/work/build/preview-data:rw','-v','/tmp/.X11-unix:/tmp/.X11-unix:ro',
  '-e','DISPLAY=:109','-e','SDL_VIDEODRIVER=x11','ps4-games-installer:build','./build/host/preview'],stdout=log,stderr=log)
 time.sleep(2);display=X.XOpenDisplay(b':109');assert display
 root=X.XDefaultRootWindow(display);parent=C.c_ulong();outroot=C.c_ulong();children=C.POINTER(C.c_ulong)();count=C.c_uint()
 for _ in range(90):
  X.XQueryTree(display,root,C.byref(outroot),C.byref(parent),C.byref(children),C.byref(count))
  if count.value:break
  if children:X.XFree(children)
  time.sleep(.5)
 assert count.value, 'Desktop app did not create its window'
 window=children[count.value-1];X.XMoveResizeWindow(display,window,0,0,1920,1080);X.XSetInputFocus(display,window,1,0);X.XFlush(display);X.XFree(children)
 def key(sym,shift=False):
  code=X.XKeysymToKeycode(display,sym);assert code
  if shift:T.XTestFakeKeyEvent(display,X.XKeysymToKeycode(display,0xffe1),1,0)
  T.XTestFakeKeyEvent(display,code,1,0);T.XTestFakeKeyEvent(display,code,0,0)
  if shift:T.XTestFakeKeyEvent(display,X.XKeysymToKeycode(display,0xffe1),0,0)
  X.XFlush(display);time.sleep(.12)
 def capture(name):
  time.sleep(.5)
  subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-f','x11grab','-video_size','1920x1080','-i',':109','-frames:v','1',str(FRAMES/(name+'.png'))],check=True)
  print('Captured actual desktop app:',name,flush=True)
 key(0xff09) # Options
 for _ in range(6):key(0xff54)
 key(0xff0d) # Settings
 key(0xff0d) # Games JSON URL keyboard
 for _ in range(80):key(0xff08)
 for char in 'http://10.1.14.10:13000/games':key(ord(char),char==':')
 capture('app-source-keyboard');key(0xff0d);capture('app-source')
 for _ in range(8):key(0xff54)
 key(0xff0d);time.sleep(2);capture('app-test')
 for _ in range(2):key(0xff54)
 key(0xff0d);time.sleep(1);capture('app-save')
 print('Saved settings:',(data/'config.json').read_text(),flush=True)
finally:
 if display:X.XCloseDisplay(display)
 if container:
  subprocess.run(['docker','rm','-f','psgi-video-app-preview'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  try:container.wait(timeout=5)
  except subprocess.TimeoutExpired:container.terminate();container.wait(timeout=5)
 xvfb.terminate();xvfb.wait(timeout=10);log.close()
