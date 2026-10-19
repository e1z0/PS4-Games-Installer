"""Capture genuine shell/browser activity for the setup tutorial; no fake outputs.
Run using the isolated tools environment described in README.md. Creates only
build/setup-video files and the dedicated Compose demonstration container.
"""
import asyncio
import codecs
import fcntl
import functools
import http.server
import json
import os
from pathlib import Path
import pty
import struct
import subprocess
import termios
import threading
import time

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / 'build/setup-video'
FRAMES = WORK / 'frames'
BROWSER = WORK / 'chromium_headless_shell-1248/chrome-headless-shell-linux64/chrome-headless-shell'

HTML = '''<!doctype html><html><head><meta charset="utf-8"><link rel="stylesheet" href="/xterm.css">
<style>body{margin:0;background:#081320;color:#dce9f7;font:24px sans-serif}header{padding:25px 44px;border-bottom:1px solid #31516b}b{color:#73ddd9}#terminal{margin:24px 40px}</style></head>
<body><header><b>REAL TERMINAL RECORDING</b> &nbsp; Linux / Docker Compose</header><div id="terminal"></div>
<script src="/xterm.js"></script><script>
window.promptCount=0;window.exitCode=0;
const term=new Terminal({cols:108,rows:27,fontSize:27,fontFamily:'DejaVu Sans Mono',lineHeight:1.13,cursorBlink:true,theme:{background:'#081320',foreground:'#e1eaf4',cursor:'#71ded8'}});
term.open(document.getElementById('terminal'));term.focus();
const ws=new WebSocket('ws://127.0.0.1:18766');
ws.onmessage=e=>term.write(e.data);term.onData(d=>ws.send(d));
term.onTitleChange(t=>{if(t.startsWith('READY:')){window.exitCode=Number(t.split(':')[1]);window.promptCount++}});
window.sendInput=t=>ws.send(t);window.terminal=term;
</script></body></html>'''

async def main():
    from websockets.asyncio.server import serve
    from playwright.async_api import async_playwright
    FRAMES.mkdir(parents=True, exist_ok=True)
    (WORK / 'terminal.html').write_text(HTML)
    (WORK / 'tutorial').mkdir(exist_ok=True)
    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH',27,108,0,0))
    env = dict(os.environ, TERM='xterm-256color', PS1='$ ', PS2='> ',
               PROMPT_COMMAND='printf "\\033]0;READY:%s\\007" "$?"')
    shell = subprocess.Popen(['/bin/bash','--noprofile','--norc'], cwd=WORK/'tutorial',
                             stdin=slave, stdout=slave, stderr=slave, env=env, start_new_session=True)
    os.close(slave)
    transcript = ''
    decoder = codecs.getincrementaldecoder('utf-8')('replace')
    peers = set()
    loop = asyncio.get_running_loop()
    def read():
        nonlocal transcript
        try: chunk = decoder.decode(os.read(master, 65536))
        except OSError: return
        transcript += chunk
        for peer in list(peers): asyncio.create_task(peer.send(chunk))
    loop.add_reader(master, read)
    async def terminal(peer):
        if peer.request.headers.get('Origin') != 'http://127.0.0.1:18765':
            await peer.close(); return
        peers.add(peer)
        await peer.send(transcript)
        try:
            async for value in peer: os.write(master, value.encode())
        finally: peers.discard(peer)
    handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=str(WORK))
    web_server = http.server.ThreadingHTTPServer(('127.0.0.1',18765),handler)
    threading.Thread(target=web_server.serve_forever,daemon=True).start()
    records = []
    try:
        async with serve(terminal,'127.0.0.1',18766), async_playwright() as pw:
            browser = await pw.chromium.launch(executable_path=str(BROWSER),args=['--no-sandbox'])
            context = await browser.new_context(viewport={'width':1920,'height':1080},
                record_video_dir=str(WORK/'recordings'), record_video_size={'width':1920,'height':1080})
            page = await context.new_page()
            origin = time.monotonic()
            await page.goto('http://127.0.0.1:18765/terminal.html')
            await page.wait_for_function('window.promptCount>0')
            async def command(key, text, timeout=180000, prompts=None):
                await page.keyboard.press('Control+l')
                await page.wait_for_timeout(200)
                before = await page.evaluate('window.promptCount')
                expected = before + (prompts if prompts is not None else text.count('\n') + 1)
                start = time.monotonic()-origin
                await page.keyboard.type(text,delay=6)
                await page.keyboard.press('Enter')
                await page.wait_for_function('(n)=>window.promptCount>=n',arg=expected,timeout=timeout)
                code=await page.evaluate('window.exitCode')
                await page.wait_for_timeout(700)
                await page.screenshot(path=str(FRAMES/(key+'.png')))
                end=time.monotonic()-origin
                records.append(dict(key=key,command=text,exit_code=code,start=start,end=end))
                print('Captured',key,'exit',code,flush=True)
                (WORK/'recorded-commands.json').write_text(json.dumps(records,indent=2))
                if code: raise RuntimeError(f'Actual command failed: {key} ({code})')
            await command('docker-version','docker compose version')
            await command('network','hostname -I')
            await command('folders','mkdir -p ps4-library/data/pkgs/games ps4-library/data/covers ps4-library/data/json\ncd ps4-library\nls -R data')
            await command('compose-file','curl -fL https://raw.githubusercontent.com/e1z0/FPKGi-node-server/main/docker-compose.example.yml -o compose.yaml\nhead -16 compose.yaml')
            await command('environment',"cat > .env <<'EOF'\nFPKGI_PUBLIC_ADDRESS=10.1.14.10:18080\nFPKGI_HTTP_PORT=18080\nFPKGI_JSON_PORT=13000\nPUID=1000\nPGID=1000\nFPKGI_SCAN_INTERVAL_SECONDS=300\nEOF\ncat .env",prompts=2)
            await command('sample-package','curl -fL https://github.com/e1z0/PS4-Games-Installer/releases/download/r40/PS4Game_installer-r40.pkg -o data/pkgs/games/PS4Game_installer-r40.pkg\nls -lh data/pkgs/games',timeout=300000)
            await command('validate-compose','docker compose -p ps4-setup-video config --quiet')
            await command('start-server','docker compose -p ps4-setup-video up -d',timeout=300000)
            for _ in range(45):
                status=await asyncio.to_thread(subprocess.run,['docker','inspect','--format','{{.State.Health.Status}}','fpkgi-node-server'],capture_output=True,text=True)
                if status.stdout.strip()=='healthy': break
                await asyncio.sleep(2)
            await command('server-health','docker compose -p ps4-setup-video ps')
            await command('server-logs','docker compose -p ps4-setup-video logs --tail 18')
            await command('server-json',"curl -f http://10.1.14.10:13000/games -o games-check.json\npython3 -m json.tool games-check.json | head -32")
            terminal_video=page.video
            await page.close()
            records_file=WORK/'terminal-video.json'
            records_file.write_text(json.dumps({'video':str(await terminal_video.path()),'scenes':records},indent=2))
            web=await context.new_page()
            pages=[
                ('docker-instructions','https://docs.docker.com/engine/install/ubuntu/'),
                ('server-project','https://github.com/e1z0/FPKGi-node-server'),
                ('installer-releases','https://github.com/e1z0/PS4-Games-Installer/releases/latest'),
                ('goldhen-project','https://github.com/GoldHEN/GoldHEN'),
                ('server-home','http://10.1.14.10:13000/'),
                ('server-library','http://10.1.14.10:13000/library/?cat=games'),
                ('library-json','http://10.1.14.10:13000/games'),
            ]
            for key,url in pages:
                response=await web.goto(url,wait_until='domcontentloaded',timeout=60000)
                await web.wait_for_timeout(1800)
                if key=='docker-instructions':
                    await web.get_by_role('heading',name='Install using the apt repository',exact=True).scroll_into_view_if_needed()
                if key=='installer-releases':
                    assets=web.get_by_text('PS4Game_installer-r40.pkg',exact=False).first
                    if await assets.count(): await assets.scroll_into_view_if_needed()
                if key=='library-json':
                    await web.evaluate("document.body.style.fontSize='22px';document.body.style.whiteSpace='pre-wrap'")
                await web.screenshot(path=str(FRAMES/(key+'.png')))
                print('Captured browser',key,response.status if response else None,flush=True)
            await context.close(); await browser.close()
    finally:
        (WORK/'terminal-transcript.txt').write_text(transcript)
        loop.remove_reader(master)
        os.write(master,b'exit\n')
        try: shell.wait(timeout=2)
        except subprocess.TimeoutExpired: shell.kill(); shell.wait()
        os.close(master); web_server.shutdown()

if __name__=='__main__': asyncio.run(main())
