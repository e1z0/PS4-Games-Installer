# Beginner setup video production

Working exports belong in ignored `dist/tutorial/`. The reviewed public movie,
companion ZIP and README preview are also stored in `docs/video/media/` so the
consumer README links work on GitHub without a separate hosting account.
Working recordings, tools, voice model and temporary renders belong in ignored
`build/setup-video/`. Application sources and the package revision are unchanged.

This recording edition uses **English narration and Ubuntu Linux**. The recording
host is Ubuntu 22.04.5. A viewer needs an already jailbroken PS4 with GoldHEN.
It is a setup tutorial, not a jailbreak or firmware-update tutorial.

## Capture provenance and limits

`capture.py` drives a real Bash pseudo-terminal and real Chromium browser, saves
screenshots and a continuous terminal recording, and records command exit codes.
The separate Compose demonstration uses ports **18080** and **13000**, project
name **ps4-setup-video**, and the official published server image. It scans the
project's own public r40 installer package; no commercial packages are downloaded.
The recording LAN IP is an example viewers must replace with their own.

Existing real PS4 pictures come from the project's `Screenshots/` directory,
dated 17 October 2026. Settings close-ups were captured here from the actual compiled r48 desktop
preview entering, testing and saving the same live demonstration server URL.
Its dedicated preview data mount keeps existing application data unchanged. These are explicitly labeled in the movie
and its source ledger. There is no attached PS4 capture device or live console
screen/control connection. USB copying and GoldHEN package installation are
explained, not newly filmed. Do not describe this export as an end-to-end newly
recorded PS4 installation, or the desktop preview as console footage.

The official Docker installation page is shown, but Docker is already installed
on the recording host; installation commands are provided for a clean Ubuntu host
rather than executed against the host's existing Docker services. The latest
public installer release at recording time is r40. Development UI captures use
r48; narration identifies this distinction and avoids promising new features in
an older public release.

## Reproduce

Use Python 3.10+, system ffmpeg with libx264/libass, DejaVu Sans fonts, and an
isolated virtual environment. Versions used:

```text
playwright==1.64.0
piper-tts==1.8.0
websockets==16.1.1
Pillow==12.2.0
```

The Chromium headless shell (build 1248) was copied from the available official
`mcr.microsoft.com/playwright:v1.64.1-noble` image. `capture.py` names its expected
path under `build/setup-video/`. Install Playwright's ffmpeg helper into
`build/setup-video/pw-cache/`, and set `PLAYWRIGHT_BROWSERS_PATH` to that directory.
For another Playwright version, use its matching browser and update that path.

Download `@xterm/xterm` 5.5.0's `lib/xterm.js` and `css/xterm.css` into the working
directory. The terminal server binds only loopback ports 18765/18766.

Narration uses Piper's `en_US-ljspeech-medium` model. Put the model at
`build/setup-video/models/voice.onnx` and its matching JSON at `voice.onnx.json`.
The [model card](https://huggingface.co/rhasspy/piper-voices/blob/main/en/en_US/ljspeech/medium/MODEL_CARD)
identifies its training dataset as public domain. Narration is synthesized, not a
recording of a person speaking this script. Credits identify the tools and model.

Start from a clean dedicated demonstration directory with no existing container
named `fpkgi-node-server`. Do not delete or stop a user's unrelated container to
make that name available. Change the recording recipe instead if it conflicts.

```sh
build/setup-video/venv/bin/python docs/video/capture.py
build/setup-video/venv/bin/python docs/video/render.py
```

`capture_app.py` uses a dedicated Xvfb display and isolated preview-data mount
for the live desktop settings demonstration. It never controls a physical PS4.

`storyboard.py` contains narration, on-screen notes and source types. `render.py`
requires every source capture, creates speech and timed captions, includes actual
terminal footage, and exports H.264/AAC 1080p MP4 with chapter markers and English captions. The visible captions are burned in for player compatibility;
SRT is also exported under `companion/` for reuse. It is kept out of the movie's
folder, and no extra text track is embedded, to prevent duplicate player captions. The source ledger hashes each screenshot. Narration
caches are reused only when their caption text matches; clear the render directory
when changing the voice model or synthesis settings. Encoded clips are reused
when their image, speech, subtitle and terminal-range inputs match the saved
receipt; clear those receipts after changing encoding/filter settings.

`commands.txt` is the consumer companion. Keep its project-name option, ports,
paths and addresses consistent with the captured commands and narration.

After filming, stop only the demonstration project, retaining its mounted files:

```sh
cd build/setup-video/tutorial/ps4-library
docker compose -p ps4-setup-video down
```

## Review before publishing

Inspect rendered terminal/settings/console frames, caption placement, chapter
boundaries, stream metadata and complete media decoding. Keep the source labels
and footage limits visible. The user requested publication of the reviewed movie
and companion notes through the project README. Other external publication, such
as uploading to YouTube, requires a separate user request.
