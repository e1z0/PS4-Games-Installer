"""Render the narrated 1080p tutorial from actual captures and a source ledger.
Requires local Piper, Pillow and ffmpeg. Never generates replacement app screens.
"""
import concurrent.futures
import hashlib
import json
from pathlib import Path
import subprocess
import textwrap
import wave
from PIL import Image, ImageDraw, ImageFont
from piper import PiperVoice, SynthesisConfig
from storyboard import SCENES

ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'build/setup-video'
OUT=ROOT/'dist/tutorial'
FONT='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
BOLD='/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'

def srt_time(seconds):
    ms=round(seconds*1000);return f'{ms//3600000:02}:{ms//60000%60:02}:{ms//1000%60:02},{ms%1000:03}'

def fit_text(draw,text,font,width):
    words=text.split();lines=[];line=''
    for word in words:
        test=(line+' '+word).strip()
        if draw.textlength(test,font=font)>width and line: lines.append(line);line=word
        else:line=test
    if line:lines.append(line)
    return lines

def frame(scene,index,source):
    key,title,_,kind,_,steps=scene
    canvas=Image.new('RGB',(1920,1080),'#081320');d=ImageDraw.Draw(canvas)
    d.rectangle((0,0,1920,5),fill='#6cddd7')
    d.text((36,22),f'{index+1:02}  {title}',font=ImageFont.truetype(BOLD,40),fill='#eef5ff')
    small=ImageFont.truetype(FONT,21)
    d.text((38,78),'   •   '.join(steps),font=small,fill='#8cddda')
    image=Image.open(source).convert('RGB')
    if kind=='terminal':crop=(24,90,image.width-24,min(image.height,1010))
    elif kind=='preview':crop=(260,110,min(image.width,1660),min(image.height,955))
    elif scene[2]=='server-library':crop=(200,0,850,500)
    elif scene[2]=='server-home':crop=(500,0,1440,800)
    else:crop=(0,0,image.width,image.height)
    image=image.crop(crop)
    image.thumbnail((1848,708),Image.Resampling.LANCZOS)
    x=(1920-image.width)//2;y=135+(708-image.height)//2
    canvas.paste(image,(x,y))
    credit={'terminal':'Captured here: real command execution / waits may be shortened',
            'browser':'Captured here: actual project website or live tutorial container',
            'console':'Real PS4 project screenshot • supplied repository capture, 17 Oct 2026',
            'preview':'Actual desktop app preview, r48 • illustration of UI, not PS4 footage'}[kind]
    d.text((38,856),credit,font=ImageFont.truetype(FONT,19),fill='#93a8bb')
    d.line((38,885,1882,885),fill='#274258',width=2)
    d.text((38,1045),'PS4 Games Installer  /  Beginner setup  /  Ubuntu Linux + existing GoldHEN',font=ImageFont.truetype(FONT,18),fill='#839bad')
    d.text((1750,1045),f'{index+1}/{len(SCENES)}',font=ImageFont.truetype(FONT,18),fill='#839bad')
    path=WORK/'render'/f'{index:02}-{key}.png';canvas.save(path)
    return path,(x,y,image.width,image.height),crop

def run_scene(job):
    index,key,png,wav,subtitle,duration,video,rect,crop=job
    output=WORK/'render'/f'{index:02}-{key}.mp4'
    digest=hashlib.sha256(b'psgi-tutorial-ass1080-v1'+(b'-bounded-live-overlay' if video else b''))
    for source_file in (png,wav,subtitle):digest.update(source_file.read_bytes())
    digest.update(json.dumps(video).encode());signature=digest.hexdigest()
    receipt=output.with_suffix('.inputs.sha256')
    if output.exists() and receipt.exists() and receipt.read_text()==signature:
        print('Reused unchanged clip',key,flush=True);return output
    command=['ffmpeg','-hide_banner','-loglevel','error','-y','-loop','1','-framerate','24','-i',str(png)]
    if video:
        movie,start,end=video
        length=max(0.25,end-start)
        command+=['-ss',str(max(0,start)),'-t',str(length),'-i',str(movie)]
        command+=['-i',str(wav)]
        x,y,w,h=rect;cx,cy,ex,ey=crop
        speed=max(1,length/max(1,duration-2))
        live=max(.1,min((length-.8)/speed,duration-2))
        graph=(f'[1:v]setpts=(PTS-STARTPTS)/{speed},crop={ex-cx}:{ey-cy}:{cx}:{cy},'
               f'scale={w}:{h},setsar=1,tpad=stop_mode=clone:stop_duration={duration}[capture];'
               f"[0:v][capture]overlay={x}:{y}:shortest=1:enable='lt(t,{live})',ass={subtitle}[v]")
        command+=['-filter_complex',graph,'-map','[v]','-map','2:a']
    else:
        command+=['-i',str(wav),'-vf',f"ass={subtitle}",'-map','0:v','-map','1:a']
    command+=['-t',str(duration),'-c:v','libx264','-preset','veryfast','-crf','21','-tune','stillimage',
              '-threads','2','-pix_fmt','yuv420p','-r','24','-c:a','aac','-b:a','160k','-ar','48000','-ac','2',str(output)]
    subprocess.run(command,check=True)
    receipt.write_text(signature)
    print('Encoded',key,round(duration,1),'seconds',flush=True)
    return output

def main():
    OUT.mkdir(parents=True,exist_ok=True);(OUT/'companion').mkdir(exist_ok=True);(WORK/'render').mkdir(exist_ok=True)
    movie_record=json.loads((WORK/'terminal-video.json').read_text())
    commands={r['key']:r for r in movie_record['scenes']}
    voice=PiperVoice.load(WORK/'models/voice.onnx')
    config=SynthesisConfig(length_scale=1.04,volume=0.85)
    jobs=[];global_captions=[];timeline=0;provenance=[];chapters=[];transcript=[];caption_sets=[]
    for index,scene in enumerate(SCENES):
        key,title,asset,kind,sentences,_=scene
        source=ROOT/asset if kind in ('console','preview') else WORK/'frames'/(asset+'.png')
        if not source.exists():raise FileNotFoundError(f'Required genuine capture is missing: {source}')
        png,rect,crop=frame(scene,index,source)
        wav=WORK/'render'/f'{index:02}-{key}.wav'
        subtitle=WORK/'render'/f'{index:02}-{key}.srt'
        captions=[]
        if wav.exists() and subtitle.exists():
            try:
                for block in subtitle.read_text().strip().split('\n\n'):
                    lines=block.splitlines(); start,end=lines[1].split(' --> ')
                    def seconds(t):
                        h,m,sec=t.replace(',', '.').split(':');return int(h)*3600+int(m)*60+float(sec)
                    captions.append((seconds(start),seconds(end),' '.join(lines[2:])))
                if [text for _,_,text in captions] != sentences: captions=[]
                with wave.open(str(wav),'rb') as stream: duration=stream.getnframes()/stream.getframerate()
            except (ValueError,IndexError,wave.Error,EOFError):captions=[]
        if not captions:
            position=.5;pieces=[b'\0'*round(22050*.5)*2]
            for line in sentences:
                chunk=WORK/'render'/'speech.wav'
                with wave.open(str(chunk),'wb') as output:voice.synthesize_wav(line,output,config)
                with wave.open(str(chunk),'rb') as stream:
                    assert stream.getframerate()==22050 and stream.getnchannels()==1
                    data=stream.readframes(stream.getnframes());line_duration=len(data)/44100
                pieces += [data,b'\0'*round(22050*.25)*2]
                captions.append((position,position+line_duration,line));position+=line_duration+.25
            pieces.append(b'\0'*22050*2)
            with wave.open(str(wav),'wb') as output:
                output.setnchannels(1);output.setsampwidth(2);output.setframerate(22050);output.writeframes(b''.join(pieces))
            duration=position+1
            subtitle.write_text(''.join(f'{n+1}\n{srt_time(a)} --> {srt_time(b)}\n'+ '\n'.join(textwrap.wrap(text,100))+'\n\n' for n,(a,b,text) in enumerate(captions)))
        ass=subtitle.with_suffix('.ass')
        ass_header = """[Script Info]
ScriptType: v4.00+
PlayResX: 1920
PlayResY: 1080
WrapStyle: 0
[V4+ Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
Style: Default,DejaVu Sans,34,&H00FFFFFF,&H00FFFFFF,&H00081320,&H00081320,0,0,0,0,100,100,0,0,1,1,0,2,48,48,64,1
[Events]
Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
"""
        def ass_time(t):return srt_time(t).replace(',', '.')[:-1]
        ass.write_text(ass_header+''.join(f'Dialogue: 0,{ass_time(a)},{ass_time(b)},Default,,0,0,0,,'+r'\N'.join(textwrap.wrap(text,100))+'\n' for a,b,text in captions))
        for a,b,text in captions:global_captions.append((timeline+a,timeline+b,text))
        video=None
        if kind=='terminal' and asset in commands:
            record=commands[asset];video=(movie_record['video'],record['start'],record['end'])
        jobs.append((index,key,png,wav,ass,duration,video,rect,crop));caption_sets.append(captions)
        chapters.append((timeline,title));timeline+=duration
        transcript += [f'## {index+1}. {title}','',*sentences,'']
        provenance.append({'scene':key,'type':kind,'source':str(source.relative_to(ROOT)),
            'sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'new_console_recording':False})
        print('Narrated',key,round(duration,1),'seconds',flush=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:outputs=list(pool.map(run_scene,jobs))
    timeline=0;global_captions=[];chapters=[]
    for scene,output,captions in zip(SCENES,outputs,caption_sets):
        actual=float(subprocess.check_output(['ffprobe','-v','error','-show_entries','format=duration','-of','csv=p=0',str(output)],text=True).strip())
        chapters.append((timeline,scene[1]))
        global_captions.extend((timeline+a,timeline+b,text) for a,b,text in captions)
        timeline+=actual
    playlist=WORK/'render'/'concat.txt';playlist.write_text(''.join(f"file '{p}'\n" for p in outputs))
    caption_file=OUT/'companion/PS4-Games-Installer-Setup.en.srt'
    caption_file.write_text(''.join(f'{n+1}\n{srt_time(a)} --> {srt_time(b)}\n'+ '\n'.join(textwrap.wrap(text,100))+'\n\n' for n,(a,b,text) in enumerate(global_captions)))
    meta=WORK/'render'/'chapters.ffmetadata';text=';FFMETADATA1\ntitle=PS4 Games Installer: Complete Beginner Setup\nartist=e1z0 / Vintage2000.org\n'
    for i,(start,title) in enumerate(chapters):
        end=chapters[i+1][0] if i+1<len(chapters) else timeline
        text+=f'[CHAPTER]\nTIMEBASE=1/1000\nSTART={round(start*1000)}\nEND={round(end*1000)}\ntitle={title}\n'
    meta.write_text(text)
    final=OUT/'PS4-Games-Installer-Setup.mp4'
    subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-f','concat','-safe','0','-i',str(playlist),
        '-i',str(meta),'-map','0:v','-map','0:a','-map_metadata','1',
        '-c','copy','-movflags','+faststart',str(final)],check=True)
    (OUT/'transcript.md').write_text('# PS4 Games Installer — Beginner setup narration\n\n'+ '\n'.join(transcript))
    (OUT/'chapters.txt').write_text('\n'.join(f'{int(t)//60:02}:{int(t)%60:02} {title}' for t,title in chapters)+'\n')
    (OUT/'capture-provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
    print('Finished:',final,'duration',round(timeline,1),'seconds',flush=True)

if __name__=='__main__':main()
