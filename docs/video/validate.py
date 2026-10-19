"""Validate the exported movie and produce chapter review contact sheets."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
from PIL import Image,ImageDraw,ImageFont

ROOT=Path(__file__).resolve().parents[2];WORK=ROOT/'build/setup-video';OUT=ROOT/'dist/tutorial'
movie=OUT/'PS4-Games-Installer-Setup.mp4'
probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-show_streams','-show_chapters','-show_format','-of','json',str(movie)]))
(WORK/'final-probe.json').write_text(json.dumps(probe,indent=2))
video=next(s for s in probe['streams'] if s['codec_type']=='video')
audio=next(s for s in probe['streams'] if s['codec_type']=='audio')
assert not any(s['codec_type']=='subtitle' for s in probe['streams']), 'Visible captions are already burned in'
assert video['codec_name']=='h264' and (video['width'],video['height'])==(1920,1080)
assert audio['codec_name']=='aac' and audio['channels']==2
chapters=probe['chapters'];assert len(chapters)==29
assert all(float(c['end_time'])>float(c['start_time']) for c in chapters)
assert all(float(a['end_time'])<=float(b['start_time'])+.005 for a,b in zip(chapters,chapters[1:]))
duration=float(probe['format']['duration']);assert 900<duration<1100
captions=(OUT/'companion/PS4-Games-Installer-Setup.en.srt').read_text()
cues=re.findall(r'(\d\d):(\d\d):(\d\d),(\d{3}) --> (\d\d):(\d\d):(\d\d),(\d{3})',captions)
assert len(cues)>90
for cue in cues:
 start=int(cue[0])*3600+int(cue[1])*60+int(cue[2])+int(cue[3])/1000
 end=int(cue[4])*3600+int(cue[5])*60+int(cue[6])+int(cue[7])/1000
 assert 0<=start<end<=duration+.1
for source in json.loads((OUT/'capture-provenance.json').read_text()):
 assert hashlib.sha256((ROOT/source['source']).read_bytes()).hexdigest()==source['sha256']
print('Streams, chapter timing, captions and source hashes validated',flush=True)
with (WORK/'final-decode-check.log').open('wb') as log:
 subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-threads','2','-i',str(movie),'-map','0:v:0','-map','0:a:0','-f','null','-'],stdout=log,stderr=log,check=True)
print('Full video/audio decode passed',flush=True)
font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',13)
canvas=Image.new('RGB',(1600,((len(chapters)+3)//4)*245),'#081320');draw=ImageDraw.Draw(canvas)
for i,chapter in enumerate(chapters):
 timestamp=float(chapter['start_time'])+min(8,(float(chapter['end_time'])-float(chapter['start_time']))/2)
 target=WORK/f'final-qc-{i:02}.png'
 subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-ss',str(timestamp),'-i',str(movie),'-frames:v','1',str(target)],check=True)
 im=Image.open(target);im.thumbnail((396,223));x=(i%4)*400;y=(i//4)*245
 canvas.paste(im,(x,y));draw.text((x+3,y+225),f'{i+1}: '+chapter['tags']['title'],font=font,fill='white')
canvas.save(WORK/'final-contact-sheet.jpg')
summary={'duration_seconds':duration,'chapters':len(chapters),'caption_cues':len(cues),
 'resolution':'1920x1080','video':'H.264','audio':'AAC stereo','full_decode_passed':True,
 'captions':'burned in, plus separate English SRT','bytes':movie.stat().st_size,'sha256':hashlib.sha256(movie.read_bytes()).hexdigest()}
(OUT/'validation.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary),flush=True)
