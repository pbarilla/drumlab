#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Writes the site's pages: index, install, play, guide, tutorials, drum patterns, cheat sheet, projects (site/**.html).
The pictures come from web/make_images.py, the samples from web/make_samples.py. Kit names and lessons are read from the firmware's
own data (firmware/gen/arch.h, kits/demos.py), so they follow it.
   python3 web/gen_site.py"""
import html, importlib.util, os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
from sitekit import *
from patterns import PATTERNS, by_name

FW_PLAY = os.path.join(ROOT, "..", "fw", "site", "play", "index.html")


# footers the author reworded by hand on two pages (kept here so a regenerate does not undo them)
FOOT = {
    "guide/index.html": '<footer>GPL-3.0-only, see <a href="https://github.com/pbarilla/drumlab/blob/main/LICENSING.md">LICENSING.md</a>. Built on <a href="../../chipbox/">Chipbox</a>, which is built on <a href="https://github.com/zvenson/dinghy">Dinghy</a>, which is built on <a href="https://github.com/hugelton/Felucca">Felucca</a> by Leo Kuroshita (Hügelton Instruments) with the changes of <a href="https://github.com/isod89/sloop-fm1">SLOOP</a> (isod89). PO-32, PO-35, and Pocket Operator belong to Teenage Engineering. Microtonic belongs to Sonic Charge. They\'re not involved in this, and are likely unaware of it. But hey, if you work there and are reading this: Hi!</footer>',
    "manual/send-po32/index.html": '<footer>GPL-3.0-only, see <a href="https://github.com/pbarilla/drumlab/blob/main/LICENSING.md">LICENSING.md</a>. Built on <a href="../../../chipbox/">Chipbox</a>, which is built on <a href="https://github.com/zvenson/dinghy">Dinghy</a>, which is built on <a href="https://github.com/hugelton/Felucca">Felucca</a> by Leo Kuroshita (Hügelton Instruments) with the changes of <a href="https://github.com/isod89/sloop-fm1">SLOOP</a> (isod89). PO-32, PO-35 and Pocket Operator belong to Teenage Engineering, who have nothing to do with this. Sonic Charge owns Microtonic, and is not involved in this. Hope they dont get mad...</footer>',
}


def write(rel, text):
    if rel in FOOT:
        text = re.sub(r"<footer>.*?</footer>", lambda m: FOOT[rel], text, count=1, flags=re.S)
    p = os.path.join(SITE, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    open(p, "w").write(text)
    print("wrote site/" + rel)


def kit_names():
    s = open(os.path.join(ROOT, "firmware", "gen", "arch.h")).read()
    return re.findall(r'\{"([A-Z0-9 -]+)", \{\{', s)


def lessons():
    spec = importlib.util.spec_from_file_location("demos", os.path.join(ROOT, "kits", "demos.py"))
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    return [l[0] for l in m.LESSONS]


KITS = kit_names()
LESSON_NAMES = lessons()
FX = ["HALF", "DIST", "SQUASH", "ECHO", "P-LFO", "EQ SW", "MORPH", "BEND", "PUNCH", "6/8", "REPEAT", "REP 2", "FM", "GRAIN", "REVRSE", "BOUNCE"]      # page 2: the PO-32's
FX1 = ["STOP", "REPEAT", "LPF", "HPF", "ECHO", "REVERB", "CRUSH", "GATE", "HALF", "DIST", "P-LFO", "BEND", "GRAIN", "REVRSE", "FM", "LATCH"]      # page 1: the shared eight, seven of ours, the latch
LABELS = {1: "Kick", 2: "Snare", 3: "Hat", 4: "Bell"}


def cap(s): return s[0].upper() + s[1:]


# ================================================================== the front page
def index():
    shots = lambda items: "".join('<figure><img src="img/%s.png" alt="%s"><figcaption>%s</figcaption></figure>' % i for i in items)
    body = f'''
  <p class="lead">A drum machine for the FM-1, built like the PO-32 tonic: 16 sounds, 16 patterns, 4 lanes. Make the beat here, then send it to a PO-32 as audio.</p>
  <p>No PO-32? It's still a drum machine.</p>


  <div class="shots">
    {shots([("playing", "The pattern view: four lanes of steps", "PATTERN"), ("snd", "The lab view: a scope and four dials", "LAB"), ("send", "The send view: a green monitor", "SEND")])}
  </div>
  <div class="shots">
    {shots([("kits", "The kits window", "KITS"), ("picker", "Twelve projects", "PROJECTS"), ("menu", "The menu", "MENU")])}
  </div>

  <h2>What is it</h2>
  <p>A simple (to me) way to make drum machine beats, the way the PO-32 does it. Three views, and you'll spend your time in them:</p>
  <ul>
    <li><b>PATTERN</b> - 16 steps, 4 lanes. Write a beat, mute a lane, throw effects at it.</li>
    <li><b>LAB</b> - every sound has 21 settings. Turn the knobs, roll a whole new kit.</li>
    <li><b>SEND</b> - plug a cable into a PO-32 and it hears all of it.</li>
  </ul>

  <h2>Example workflow</h2>
  <ul>
    <li>Hold HOME, pick a NEW project, tap HOME to open it.</li>
    <li>Tap REC and press keys to write steps. PRESETS picks the sound. Hit PLAY.</li>
    <li>Hit HOME to flip to LAB. Turn ALGORITHM to flip pages, turn the knobs. Hit OCT+ to roll a kit.</li>
    <li>Hit HOME until SEND is visible. Hit PLAY, then OCT+. The PO-32 does the rest.</li>
  </ul>
  <p>Never touched a drum machine? Hit GLO + HOME, pick LESSONS and play along with the rings.</p>

  <h2>The three brothers</h2>
  <div class="shelf" id="shelf"></div>
  <p style="margin-top:14px">They get along: over Bluetooth one of them leads and the others follow. Tempo, kicks, patterns and effects. <a href="manual/sync/">Here's how</a>.</p>

  <h2 id="install">Install</h2>
  <p><a href="install.html">install it from your browser</a>, or <a id="download" href="#install">download the package</a>.</p>

  <h2>Tested?</h2>
  <ul><li>Yes, on a real FM-1. Not every corner has met a real PO-32 yet.</li></ul>
'''
    scripts = '''<script>window.BRO_ME = "drumlab";</script>
<script src="js/boxes.js"></script>
<script>
  (function () {
    var base = %r;
    document.getElementById('download').href = base + '/releases/latest';
  })();
</script>
''' % REPO
    write("index.html", page("Drumlab", body, 0, "Home", desc="A drum machine for the FM-1 that works like the PO-32 tonic and sends its sounds and patterns to one, as audio.", scripts=scripts, head='<link rel="stylesheet" href="css/boxes.css">\n'))


# ================================================================== install
def install():
    css = '''<style>
  main { max-width:640px; }
  progress { width:100%; height:8px; margin-top:20px; accent-color:var(--coral); }
  #status { margin-top:12px; min-height:1.55em; }
  #version { color:var(--dim); margin-left:14px; }
  .small { color:var(--dim); font-size:14px; }
  #log { color:var(--dim); font:inherit; font-size:12px; white-space:pre-wrap; margin-top:16px; }
</style>
'''
    body = '''
  <p>Plug in the FM-1.</p>
  <p><button class="btn hot" id="go" disabled>Install</button><span id="version"></span></p>
  <progress id="bar" max="100" value="0"></progress>
  <p id="status"></p>
  <p class="small">Don't unplug it till it's done.</p>
  <p class="small">Installer by <a href="https://github.com/hugelton/Felucca">Felucca</a>. <a href="./">Back</a></p>
  <pre id="log"></pre>
'''
    write("install.html", page("Install Drumlab", body, 0, "Install", tag="INSTALL", head=css, scripts='<script type="module" src="js/install.js"></script>\n'))


# ================================================================== play online
def play():
    src = open(FW_PLAY).read()
    style = re.search(r"<style>(.*?)</style>", src, re.S).group(1)
    style = style[style.index(".devicebox"):]
    style = style.replace("rgba(14,14,18,.6)", "rgba(26,24,34,.6)")
    css = '''<style>
  :root { --panel:#262430; --deep:#1a1822; --red:#f06446; --blue:#ffa03c; }
  main { max-width:1040px; }
  .start button { border-radius:8px; }
  %s</style>
''' % style
    dev = re.search(r'<div class="devicebox">.*?<span id="midi"></span>\n.*?</div>\n', src, re.S).group(0)
    js = re.search(r"<script>\n\(function \(\) \{.*?\}\)\(\);\n</script>", src, re.S).group(0)
    js = js.replace('"chipbox-flash-1"', '"drumlab-flash-1"').replace("Chipbox()", "Drumlab()")
    assert "Drumlab()" in js and "drumlab-flash-1" in js
    scripts = '<script src="drumlab.js"></script>\n' + js + "\n"
    write("play/index.html", page("Play online · Drumlab", "\n" + dev, 1, "Play", tag="PLAY ONLINE", head=css, scripts=scripts, desc="Play Drumlab in your browser."))


# ================================================================== projects
def projects():
    css = '''<style>
  main { max-width:680px; }
  button, .row .btn { font:700 13px var(--display); letter-spacing:.1em; text-transform:uppercase; background:var(--cream); color:#1a1822; border:0; border-radius:6px; padding:9px 14px; cursor:pointer; text-decoration:none; }
  button.alt, .btn.alt { background:var(--plate); color:var(--cream); box-shadow:none; border:1px solid #4a4658; }
  button.warn { background:var(--coral); color:#fff; }
  .row { display:flex; flex-wrap:wrap; align-items:center; gap:8px 12px; padding:12px 14px; background:var(--plate); border-radius:8px; margin-bottom:6px; }
  .row .n { width:2ch; color:var(--dim); }
  .row .what { flex:1 1 180px; min-width:0; }
  .row .what small { display:block; color:var(--dim); font-size:13px; }
  .row.open { outline:1px solid var(--mint); }
  .btns { margin:0; gap:6px; }
  select { font:inherit; font-size:14px; background:var(--bg); color:var(--cream); border:1px solid #4a4658; border-radius:6px; padding:7px 8px; }
  #status { margin:16px 0; min-height:1.55em; }
  #bar { width:100%; height:8px; display:none; accent-color:var(--coral); }
  input[type=file] { display:none; }
  body.busy button, body.busy .btn, body.busy select { opacity:.4; pointer-events:none; }
</style>
'''
    body = '''
  <p><button id="connect">Connect the FM-1</button></p>
  <progress id="bar" max="100" value="0"></progress>
  <p id="status"></p>

  <div id="mine" hidden>
    <div id="slots"></div>
    <p class="btns" style="margin-top:14px"><button id="backup" class="alt">Back up all</button> <button id="restore" class="alt">Restore backup</button></p>
    <p id="confirm" hidden></p>
  </div>

  <h2>Samples</h2>
  <div id="samples"></div>
  <input type="file" id="pick" accept=".drumlab,.json">
'''
    write("projects/index.html", page("Projects · Drumlab", body, 1, "Projects", tag="PROJECTS", head=css, scripts='<script type="module" src="../js/projects.js"></script>\n'))


# ================================================================== the manual
def pad_map():
    rows = []
    for r in range(4):
        rows.append("".join('<td style="text-align:center"><code>%d</code></td>' % (r * 4 + c + 1) for c in range(4)))
    return ('<table style="width:auto"><tr><th style="text-align:center">Lane 1</th><th style="text-align:center">Lane 2</th><th style="text-align:center">Lane 3</th><th style="text-align:center">Lane 4</th></tr>'
            + "".join("<tr>%s</tr>" % r for r in rows) + "</table>")


def fx_map():
    rows = []
    for r in range(4):
        rows.append("".join('<td style="text-align:center"><code>%s</code></td>' % FX1[r * 4 + c] for c in range(4)))
    return '<table style="width:auto">' + "".join("<tr>%s</tr>" % r for r in rows) + "</table>"


def guide():
    d = Doc("../manual/")
    I = d.item
    d.section("The three views",
        I("`HOME` flips between PATTERN, LAB and SEND.", "g-home"),
        I("PATTERN is where you write the beat, LAB is where you shape the sounds, and SEND lets you push your masterpiece to a PO-32 or PO-35."),
        I("Hold `HOME` to open the projects."),
        I("`GLO` shows the help for the view you're on.", "g-help"),
        I("`GLO` + `HOME` opens the menu."),
        I("`PLAY` plays and stops. `SAVE` saves."), id="views")
    d.section("Sounds and lanes",
        I("There are 16 sounds, on the 16 white keys as a 4 x 4 grid.", "g-presets"),
        d.block(pad_map()),
        I("The columns are the 4 lanes. A lane plays one sound at a time, so a new hit cuts the old one."),
        I("`PRESETS` picks a sound. The keys play them."),
        I("Sounds 1 to 4 are a kick, a snare, a hat, and then 'something else' in every kit. This is just a recommendation, feel free to break up the sounds however you like!"), id="sounds")
    d.section("Writing steps",
        I("Tap `REC` to turn WRITE on. The 16 keys represent the 16 steps.", "g-write"),
        I("Pick a sound with `PRESETS`, then press the steps it plays on. Press a step again to take it away. So far so good. Basically like every single drum machine ever made!", "g-steps"),
        I("A step holds one sound per lane. Writing another sound of the same lane replaces it."),
        I("Hold `REC` while it plays and press the pads. The hits land on the step nearest the playhead. AKA live recording mode!"),
        I("`SELECT` picks the pattern, 1 to 16.", "g-pattern"),
        I("`OCT+` copies the pattern to the next one."),
        I("Hold `EDIT` to clear the pattern."),
        I("`LFO` takes back the last change to the pattern. One step back, and no redo. Kind of like 1 chance!"),
        I("Drumlab autosaves when it can, but if you really wanna be sure, then stop playing and dont touch anything for around 5 seconds!", "g-save"), id="steps")
    d.section("Accents",
        I("`ENV` turns ACCENT on. Press a step to accent it, or...unaccent it? Is that the right word. Unaccent. Sounds weird.", "g-accent"),
        I("An accent is on the step, so every lane's hit on it is louder. Be careful of those random cowbells!"), id="accents")
    d.section("Mute and solo",
        I("`ARP` once is MUTE, twice is SOLO, three times is off.", "g-mute"),
        I("In either, a key picks its lane: keys 1, 5, 9 and 13 are lane 1, and so on. That's confusing isnt it? Just do 1,2,3,4. Whatever. I should probably review that...", "g-solo"), id="mute")
    d.section("Chain",
        I("`SEL` turns chain mode on. The keys add patterns 1 to 16 to the chain.", "g-chain"),
        I("Tap `EDIT` to take the last one off. Hold `EDIT` to clear the chain."),
        I("With no chain, the pattern on show loops. Which is kind of obvious when you think about it, huh. Like a chain of length 1. But hey, gotta write it here! Future generations, and all that..."),
        I("A chain holds up to 64 patterns. That's alotta patterns!"), id="chain")
    d.section("Tempo and swing",
        I("`KNOB 1` is the tempo, 60 to 240. Why did I limit it to 60, you may ask? Oh, no reason... nothing to do with a minute having 60 seconds, and a second having 1000ms, and it being all nice and easy to figure out...", "g-tempo"),
        I("`KNOB 2` is the swing, 0 to 12.", "g-swing"), id="tempo")
    d.section("Pitch and morph",
        I("`KNOB 3` tunes the sound you picked and `KNOB 4` slides it from patch A to patch B.", "g-pm"),
        I("A step keeps the pitch and morph it was written with."),
        I("To change one step, hold its key in WRITE and turn `KNOB 3` or `KNOB 4`. That's a step lock, baby!"),
        I("To change many, hold `REC` while it plays and turn the knobs. They write into the steps of the sound you picked as they pass."), id="locks")
    d.section("Effects",
        I("Tap `FX` and the 16 white keys become effects. Hold a key for its effect. Tap `FX` again to put them away. Or hold `FX` for the layer while you hold it.", "g-fx"),
        d.block(fx_map()),
        I("Pads 1 to 8 are the same on Chipbox, Smileyface and Drumlab, and they travel over Bluetooth: STOP, REPEAT, LPF, HPF, ECHO, REVERB, CRUSH and GATE."),
        I("Pads 9 to 15 only exist on Drumlab: HALF, DIST, P-LFO, BEND, GRAIN, REVRSE and FM. They are the PO-32's effects, so with WRITE on and the pattern playing, they are written into the steps as they pass."),
        I("Pad 16 is the LATCH. With it on, a tap holds a pad on and the next tap lets it go."),
        I("With WRITE on, hold `FX` and press nothing while it plays to wipe the effects from the steps as they pass."), id="fx")
    d.section("The LAB view",
        I("`PRESETS` picks the sound.", "g-knobs"),
        I("`ALGORITHM` flips through six pages. The four knobs set what the page shows."),
        d.block(table(["Page", "KNOB 1", "KNOB 2", "KNOB 3", "KNOB 4"], [
            ["OSC", "WAVE: SIN, TRI, SAW", "FREQ", "ATTACK", "DECAY"],
            ["MOD", "MODE: DECAY, SINE, NOISE", "RATE", "AMOUNT", "VELOC"],
            ["NOISE", "FILTER: LP, BP, HP", "FREQ", "Q", "VELOC"],
            ["ENV", "SHAPE: EXP, LIN, MOD", "ATTACK", "DECAY", "MIX"],
            ["OUT", "DIST", "EQ HZ", "EQ DB", "LEVEL"],
            ["SOUND", "VELOC", "PITCH", "MORPH", ""]])),
        I("The sound plays a moment after you stop turning. The keys play the sound as you have it now."),
        I("Hold `EDIT` while you turn for fine control: a click moves a tenth as far."),
        I("`PLAY` loops the sound while you tweak.", "g-loop"), id="sound")
    d.section("Patches A and B",
        I("Every sound is two patches. `SEL` picks the one you edit.", "g-ab"),
        I("MORPH, on the SOUND page and on `KNOB 4`, blends the two when the sound plays."),
        I("`MORPH` in the FX layer plays every hit as patch B."), id="ab")
    kits = ", ".join("<code>%s</code>" % k for k in KITS)
    d.section("Kits",
        I("In the LAB view, `OCT+` opens the kits window. Every kit replaces all 16 sounds. Your patterns stay.", "g-kits"),
        I("`SELECT` or `PRESETS` walks the list: a rolled kit, LAB-1 and the preset kits. Each plays as you land on it, and the keys play its sounds.", "g-kitlist"),
        d.block("<p>Preset kits: %s.</p>" % kits),
        I("On the rolled kit, `PLAY` rolls another."),
        I("`OCT+` keeps the kit. `OCT-` puts the old one back."), id="kits")
    d.section("Sending to a PO-32, PO-35, etc",
        I("In the SEND view, hit the `PLAY` button. Now thank me later, but a warning appears.", "g-send"),
        I("`OCT+` sends all 16 sounds, A and B, the 16 patterns, the tempo, the swing and the chain.", "g-ask"),
        I("The <a href=\"../manual/send-po32/\">tutorial</a> has more information!"), id="send")
    d.section("Projects",
        I("Hold `HOME` to open the projects. There are 12.", "g-picker"),
        I("`SELECT` or `PRESETS` to pick one!"),
        I("`HOME` opens it. Hold `HOME` to close the list and stay where you were."),
        I("Hold `EDIT` to delete the selected pattern.", "g-delete"),
        I("A new project starts from the LAB-1 kit. Whats the LAB-1 kit? Oh... you know."), id="projects")
    d.section("The menu",
        I("`GLO` + `HOME` opens it. `SELECT` or `PRESETS` picks a row, `OCT+` and `OCT-` change it, `HOME` leaves.", "g-menu"),
        d.block(table(["Row", "What it does"], [
            ["MIDI CLOCK", "OFF, SEND or FOLLOW."],
            ["MIDI CHANNEL", "ANY, or 1 to 16. Only notes and controllers from that channel come in."],
            ["LESSONS", "`KNOB 1` picks one, `OCT+` starts it."],
            ["BATTERY", "How full it is."],
            ["FACTORY RESET", "`OCT+` asks. Yes erases the projects and puts the demos back."],
            ["ABOUT", "The firmware's name."]])),
        I("SEND makes Drumlab the clock: 24 pulses a quarter note, with start and stop. FOLLOW takes the tempo from the clock coming in.", "g-clock"), id="menu")
    les = ", ".join("<code>%s</code>" % n for n in LESSON_NAMES)
    d.section("Lessons",
        I("A lesson shows a pattern as rings to play in. Press the steps the rings are on. Like guitar hero, but slower, not a guitar, and not very heroic. But it'll teach ya how to use Drumlab!", "l-start"),
        d.block("<p>%s.</p>" % les),
        I("`HOME` leaves. Your project comes back as it was."),
        I("When every ring is filled, `OCT+` starts the next lesson."), id="lessons")
    d.section("MIDI",
        d.block(table(["Message", "What it does"], [
            ["Notes 0 to 15, and 36 to 51", "Play the 16 sounds. On 36 to 51, a velocity over 110 is an accent."],
            ["Notes out", "The pads send notes 36 to 51 on channel 10 whenever they sound."],
            ["Program change 0 to 15", "Picks the pattern."],
            ["CC 102", "64 and up plays, below stops."],
            ["CC 103", "The tempo, divided by 2."],
            ["CC 104", "A value of 1 sends to the PO-32."],
            ["Clock, start, stop", "Out when MIDI CLOCK is SEND, in when it is FOLLOW."]])),
        I("While it follows a clock, the tempo turns cyan."), id="midi")
    d.section("Backup and restore",
        I("The <a href=\"../projects/\">Projects page</a> downloads any project, or all of them in one file. It puts them back the same way."),
        I("A project is a <code>.drumlab</code> file. A backup of everything is one <code>.json</code>."), id="backup")
    d.section("The numbers",
        d.block(table(["", ""], [["Sounds", "16, each with patches A and B"], ["Lanes", "4"], ["Patterns", "16"], ["Steps in a pattern", "16"],
                                 ["Chain", "64 patterns"], ["Effects", "16"], ["Tempo", "60 to 240 bpm"], ["Swing", "0 to 12"], ["Projects", "12"], ["Undo", "1 change"]])), id="numbers")
    d.section("Updates and rescue",
        I("Install and update with the <a href=\"../install.html\">install page</a>."),
        I("If your FM-1 is stuck, hold `OCT-` while you plug it in. The install page takes it from there."), id="rescue")
    toc = "".join('<a href="#%s">%s</a>' % t for t in [("views", "Views"), ("sounds", "Sounds"), ("steps", "Steps"), ("accents", "Accents"), ("mute", "Mute"), ("chain", "Chain"),
                                                      ("tempo", "Tempo"), ("locks", "Locks"), ("fx", "Effects"), ("sound", "LAB"), ("ab", "A and B"), ("kits", "Kits"),
                                                      ("send", "Send"), ("projects", "Projects"), ("menu", "Menu"), ("lessons", "Lessons"), ("midi", "MIDI"), ("backup", "Backup"), ("numbers", "Numbers"), ("rescue", "Rescue")])
    write("guide/index.html", page("Manual · Drumlab", '  <div class="toc">%s</div>\n\n%s\n' % (toc, d.html()), 1, "Manual", tag="MANUAL"))


# ================================================================== the tutorials
EXP = {"first-beat": 1, "make-a-kit": 2, "send-po32": 3, "sync": 4, "patterns": 5}


def tutorial(slug, title, intro, build):
    d = Doc("../../manual/")
    d.img_dir = "../"
    build(d)
    body = '  <p class="intro">%s</p>\n\n%s\n' % (pills(intro), d.html())
    write("manual/%s/index.html" % slug, page(title + " · Drumlab", body, 2, "Tutorials", tag="TUTORIAL", exp=EXP.get(slug)))


def t_first(d):
    I = d.item
    d.section("The whole thing", d.block(grid({1: [1, 5, 9, 13], 2: [5, 13], 3: [3, 7, 11, 15]}, [5, 13])))
    d.section("A new project",
        I("Hold `HOME` to open the projects.", "t-picker"),
        I("Turn `SELECT` to a NEW one.", "t-new"),
        I("Tap `HOME` to open it.", "t-open"))
    d.section("The kick",
        I("Tap `REC`. Now the 16 keys are the 16 steps.", "t-write"),
        I("Press keys 1, 5, 9 and 13. That's the kick on every beat.", "t-kick"),
        I("<b>PROTIP:</b> Press a step again to take it away. Hit `PLAY` first if you want to hear it as you build it."))
    d.section("The snare",
        I("Turn `PRESETS` once to pick sound 2.", "t-presets"),
        I("Press keys 5 and 13.", "t-snare"))
    d.section("The hats",
        I("Turn `PRESETS` once more, for sound 3. Press keys 3, 7, 11 and 15. Right between the kicks.", "t-hat"))
    d.section("Play it",
        I("Tap `REC` to leave WRITE, then hit `PLAY`. That's a house beat. You made that!", "t-play"))
    d.section("Make it yours",
        I("Turn `KNOB 1` up to 128.", "t-tempo"))
    d.section("Accents",
        I("Tap `ENV`, then keys 5 and 13. The ring means louder.", "t-env"),
        I("Tap `ENV` again when you're done.", "t-accent"))
    d.section("Keep it",
        I("Hit `PLAY` to stop, then `SAVE`. It would have saved itself a few seconds later, but now you know.", "t-save"))


def t_kit(d):
    I = d.item
    d.section("Roll a kit",
        I("Tap `HOME` to open the LAB view.", "k-home"),
        I("Hit `OCT+`. A kit rolls, and plays itself.", "k-octup"),
        I("Hit `PLAY` to roll the dice again. Keep going until you hear one you like. The keys play its sounds.", "k-roll"))
    d.section("Preset kits",
        I("Turn `SELECT` for LAB-1 and the preset kits: %s." % ", ".join(KITS), "k-select"))
    d.section("Keep it",
        I("Hit `OCT+` to keep the kit. `OCT-` puts the old one back.", "k-keep"),
        I("The beat from the first tutorial plays on it, if you hit `PLAY` in the PATTERN view.", None))
    d.section("Tweak a sound",
        I("Press a key to pick a sound. Turn `KNOB 2` to change its FREQ.", "k-freq"),
        I("Turn `ALGORITHM` for the next page.", "k-algo"),
        I("Hit `SEL` for the sound's other patch. Every sound is two, A and B.", "k-sel"),
        I("Hit `PLAY` to loop the sound while you tweak.", "k-loop"))
    d.sections.append('  <section class="step">\n    <h2><b>4.5</b>Ohh... now i get it!</h2>\n    <div class="item"><div class="t"><div>So I \'roll a kit\' like in step 1, and if it\'s close to what im thinking, I take it and edit it to make it my own. If it sucks I roll again. But I can always edit the crap out of anything and make it sound like I want! Got it!</div></div></div>\n  </section>')
    d.section("Hear it in the beat",
        I("Tap `HOME` twice to get back to PATTERN, then hit `PLAY`.", "k-again"))


def t_send(d):
    I = d.item
    d.section("Plug in",
        I("Plug a cable from the FM-1's output into the PO-32 or PO-35's input. Turn `MASTER` up."),
        I("Get the PO-32 listening: <code class=\"po\">acc</code> + <code class=\"po\">sound</code>."))
    d.section("Send it",
        I("Tap `HOME` until you're in the SEND view.", "s-home"),
        I("Hit `PLAY`.", "s-play"),
        I("Hit `OCT+`. All 16 sounds, all 16 patterns, the tempo and the swing go over. Maybe plug in a spare 3.5mm cable into the output of the Pocket Operator, cause it's LOUD!", "s-yes"),
        I("`PLAY` stops it. But like, dont stop it, it'll corrupt whatever is on the Pocket Operator! Why did I even add the option to stop sending?"))
    d.section("Play it there",
        I("Press <code class=\"po\">play</code> on the Pocket Operator. That's your beat."))


def t_smiley(d):
    I = d.item
    d.section("Pick the leader",
        I("Hit `GLO` + `HOME` for the menu, pick BLUETOOTH and hit `OCT+`."),
        I("On the leader, set SYNC to LEAD. On the others, set it to FOLLOW. `OCT+` changes it."),
        I("Any of Drumlab, Chipbox and Smileyface can lead. One at a time."),
        I("STATUS says SEARCHING while a follower looks, LINKED when it hears the leader, and IN SYNC when it plays along."))
    d.section("Play",
        I("Hit `PLAY` on the leader. The followers start at the next bar and take its tempo."),
        I("Change the leader's tempo and they follow. `PLAY` on the leader stops them all."),
        I("No cables and no pairing. Keep them in the same room."))
    d.section("What else comes along",
        I("SIDECHAIN: the leader says where its kicks are. The followers duck on each one and swell back, so the bass pumps against the beat. Drumlab sends its first lane."),
        I("PATTERNS: when the leader changes pattern, the followers change at the end of their loop. Its swing comes along too."),
        I("PUNCH-IN FX: hold one of pads 1 to 8 on page 1 of the effects layer on the leader and the followers do it too: STOP, REPEAT, LPF, HPF, ECHO, REVERB, CRUSH and GATE. Pads 9 to 15 are different on each FM-1 and don't travel."),
        I("KEY: Chipbox and Smileyface work out the key of what they play. A following one moves its notes to the leader's key. Drumlab has no notes, so it has no KEY row."))
    d.section("The demo",
        I("DEMO loads a locked project: D minor, 124 BPM. Load it on each brother, make one the leader, and hit `PLAY`."),
        I("Nothing in it is saved. Open any project to leave it."))
    d.section("Or use MIDI clock",
        I("MIDI CLOCK in the menu sends or follows the standard MIDI clock over USB. Use it with a DAW or another box."),
        I("Plug the FM-1s into your computer and route the leader's MIDI out to each follower's MIDI in. Pass only the clock, start and stop."),
        I("Before you install a new firmware, set SYNC to OFF. While the radio runs, the FM-1 can stop answering the installer."))


def t_patterns(d):
    d.sections.append('  <section class="step">\n    <p class="intro" style="margin:0 0 6px">Sounds 1 to 4 are the top row of the pads: a kick, a snare, a hat and a bell, tom or cowbell. Every pattern here uses only them. Write one with <code>REC</code>: pick the sound with <code>PRESETS</code>, then press the steps.</p>\n  </section>')
    for name, bpm, sw, what, hits, acc in PATTERNS:
        lines = " &middot; ".join("sound %d: %s" % (s, " ".join(map(str, st))) for s, st in sorted(hits.items()))
        a = ("accents %s &middot; " % " ".join(map(str, acc))) if acc else ""
        names = {0: "Kick", 1: "Snare", 2: "Hat", 3: "Bell"}
        d.sections.append('  <section class="pcard">\n    <h3>%s</h3>\n    <p class="meta">%s</p>\n    %s\n    <p class="how">%s%d bpm, swing %d<br>%s</p>\n  </section>'
                          % (name, html.escape(what), grid(hits, acc, names), a, bpm, sw, lines))
    d.sections.append('  <section class="step">\n    <h2><b>&#9835;</b>Lessons on the device</h2>\n%s\n%s\n  </section>' % (
        d.item("The menu has these grooves as lessons. Hit `GLO` + `HOME`, turn `SELECT` to LESSONS, pick one with `KNOB 1` and hit `OCT+`.", "l-row"),
        d.item("Rings show where the hits go. Press the steps to fill them.", "l-half")))


def tutorials():
    tutorial("first-beat", "Your first beat", "A house beat in about two minutes: kick, snare, hat. Start here.", t_first)
    tutorial("make-a-kit", "Make a kit", "How the heck do we make 'infinite sounds' with a $67 synth and less than 1mb of space?", t_kit)
    tutorial("send-po32", "Send to a PO-32", "Sending your masterpiece to a Pocket Operator.", t_send)
    tutorial("sync", "Sync your FM-1s", "Chipbox, Smileyface and Drumlab over Bluetooth: one leads, the others follow. Tempo, kicks, patterns, effects, key.", t_smiley)
    # the patterns page
    d = Doc("../")
    d.img_dir = "../"
    t_patterns(d)
    intro = ('Common drum machine grooves, ready to write in. The idea is from <a href="https://shittyrecording.studio">Pocket Operations</a> by Shittyrecording.studio, '
             'a book of patterns for pocket machines. These patterns are our own.')
    body = '  <p class="intro">%s</p>\n\n%s\n' % (intro, d.html())
    write("manual/patterns/index.html", page("Drum patterns · Drumlab", body, 2, "Tutorials", tag="DRUM PATTERNS", exp=EXP["patterns"]))
    # the list
    items = [("first-beat/", "Your first beat", "A house beat on the device, step by step. Start here."),
             ("make-a-kit/", "Make a kit", "Roll a kit, keep it, tweak a sound."),
             ("send-po32/", "Send to a PO-32", "Sounds and patterns over a cable."),
             ("sync/", "Sync your FM-1s", "One leads over Bluetooth, the others follow."),
             ("patterns/", "Drum patterns", "%d grooves to write in: four on the floor, boom bap, trap, and more." % len(PATTERNS))]
    d = Doc("")
    for i, (href, t, s) in enumerate(items, 1):
        d.section(t, d.item(s), link=href)
    write("manual/index.html", page("Tutorials · Drumlab", "\n" + d.html() + "\n", 1, "Tutorials", tag="TUTORIALS"))


# ================================================================== the cheat sheet
X0, Y0, W, H = 30, 82, 1040, 664
KNOBS = [("MASTER", 139, 187), ("SELECT", 253, 185), ("PRESETS", 139, 295), ("ALGORITHM", 253, 295), ("KNOB1", 665, 185), ("KNOB2", 772, 185), ("KNOB3", 880, 185), ("KNOB4", 988, 185)]
BTNS = [("OCT-", 147, 380, 84), ("OCT+", 249, 380, 84), ("FX", 663, 287, 58), ("SEL", 729, 287, 58), ("ENV", 794, 287, 58), ("LFO", 860, 287, 58), ("EDIT", 925, 287, 58),
        ("GLO", 990, 287, 58), ("HOME", 663, 352, 58), ("SAVE", 729, 352, 58), ("ARP", 794, 352, 58), ("SEQ", 860, 352, 58), ("PLAY", 925, 352, 58), ("REC", 990, 352, 58)]
BLACKK = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25}
# the one list of what each control does: change a control in the firmware, change it here, then python3 web/gen_site.py
CONTROLS = [
    ("MASTER", "Volume", "Volume", "Volume", ""),
    ("SELECT", "Pattern", "", "", "pattern"),
    ("PRESETS", "Sound", "Sound", "", "sound"),
    ("ALGORITHM", "", "Page", "", "page"),
    ("KNOB 1", "Tempo", "Setting 1", "", "tempo"),
    ("KNOB 2", "Swing", "Setting 2", "", "swing"),
    ("KNOB 3", "Pitch. Step held: its pitch", "Setting 3", "", "pitch"),
    ("KNOB 4", "Morph. Step held: its morph", "Setting 4", "", "morph"),
    ("OCT-", "", "", "", ""),
    ("OCT+", "Copy the pattern to the next", "Kits", "", "kits"),
    ("FX", "Tap: effects", "", "", "effects"),
    ("SEL", "Chain", "Patch A / B", "", "chain"),
    ("ENV", "Accent", "", "", "accent"),
    ("LFO", "Undo", "", "", "undo"),
    ("EDIT", "Hold: clear. Chain: tap takes the last off", "Hold: fine knobs", "", "clear"),
    ("GLO", "Help", "Help", "Help", "help"),
    ("HOME", "Next view. Hold: projects", "Next view. Hold: projects", "Next view. Hold: projects", "view"),
    ("SAVE", "Save", "Save", "Save", ""),
    ("ARP", "Mute, solo, off", "", "", "mute"),
    ("SEQ", "", "", "", ""),
    ("PLAY", "Play / stop", "Loop the sound", "Send (asks first). Stop", ""),
    ("REC", "Tap: WRITE. Hold while playing: punch in", "", "", "write"),
    ("KEYS", "16 sounds. WRITE: 16 steps", "Play the sound", "", ""),
]
WINDOWS = [
    ("Projects (hold HOME)", "SELECT / PRESETS choose. HOME opens. Hold HOME: close. Hold EDIT: delete, OCT+ yes."),
    ("Kits (LAB, OCT+)", "SELECT / PRESETS choose. PLAY rolls the dice. OCT+ keeps. OCT- puts the old kit back."),
    ("Send (SEND, PLAY)", "OCT+ sends. Anything else puts it away."),
    ("Menu (GLO + HOME)", "SELECT / PRESETS pick a row. OCT+ / OCT- change it. HOME leaves."),
    ("Lessons (menu)", "KNOB 1 picks. OCT+ starts. HOME leaves. Done: OCT+ for the next."),
]


def cheat_panel():
    short = {c[0].replace(" ", ""): c[4] for c in CONTROLS}
    o = ['<svg viewBox="%d %d %d %d" class="panel" role="img" aria-label="The FM-1 and what its controls do">' % (X0, Y0, W, H)]
    o.append('<rect x="32" y="84" width="1036" height="660" rx="46" class="body"/><rect x="342" y="127" width="246" height="246" rx="6" class="tray"/>')
    o.append('<rect x="90" y="349" width="218" height="59" rx="10" class="tray"/><rect x="621" y="247" width="416" height="146" rx="14" class="tray"/><rect x="67" y="435" width="966" height="280" rx="14" class="tray"/>')
    o.append('<rect x="361" y="145" width="208" height="208" class="screen"/><text x="465" y="257" class="screentext">PATTERN</text>')
    for name, cx, cy in KNOBS:
        o.append('<circle cx="%d" cy="%d" r="29" class="knob"/><line x1="%d" y1="%d" x2="%d" y2="%d" class="notch"/>' % (cx, cy, cx, cy - 6, cx, cy - 22))
        o.append('<text x="%d" y="%d" class="lab">%s</text>' % (cx, cy - 41, name.replace("KNOB", "KNOB ")))
        s = short.get(name, "")
        if s: o.append('<text x="%d" y="%d" class="role">%s</text>' % (cx, cy + 50, html.escape(s)))
    for name, cx, cy, w in BTNS:
        s = short.get(name, "")
        o.append('<rect x="%g" y="%d" width="%d" height="42" rx="9" class="btn%s"/>' % (cx - w / 2, cy - 21, w, "" if s else " free"))
        o.append('<text x="%d" y="%d" class="btxt">%s</text>' % (cx, cy + 5, name))
        if s: o.append('<text x="%d" y="%d" class="role">%s</text>' % (cx, cy + 36, html.escape(s)))
    wi = 0
    for k in range(27):
        if k in BLACKK:
            o.append('<rect x="%g" y="452" width="46" height="94" rx="23" class="key black"/>' % (109 + (wi - 0.5) * 58.7 - 23))
        else:
            o.append('<rect x="%g" y="553" width="46" height="148" rx="23" class="key"/>' % (109 + wi * 58.7 - 23)); wi += 1
    o.append('<text x="550" y="690" class="role big">16 white keys: sounds or steps</text></svg>')
    return "".join(o)


def cheatsheet():
    rows = "".join("<tr><th>%s</th>%s</tr>" % (n, "".join("<td>%s</td>" % (html.escape(v) or '<span class="none">·</span>') for v in (a, b, c))) for n, a, b, c, _ in CONTROLS)
    wins = "".join("<tr><th>%s</th><td>%s</td></tr>" % (html.escape(a), html.escape(b)) for a, b in WINDOWS)
    pads = "".join('<span class="pad p%d">%s</span>' % (i % 4, n) for i, n in enumerate(FX1))
    css = '''<style>
  main { max-width:1100px; }
  .panel { width:100%; height:auto; display:block; margin:6px 0 18px; }
  .panel .body { fill:#262430; stroke:#4a4658; stroke-width:4; } .panel .tray { fill:#1a1822; } .panel .screen { fill:#000; }
  .panel .knob { fill:#1a1822; stroke:#4a4658; stroke-width:4; } .panel .notch { stroke:#eee4c6; stroke-width:5; stroke-linecap:round; }
  .panel .btn { fill:#2e2c3a; stroke:#4a4658; stroke-width:2; } .panel .btn.free { opacity:.45; }
  .panel .key { fill:#5c586a; } .panel .key.black { fill:#46425a; }
  .panel text { text-anchor:middle; font-family:inherit; font-weight:700; }
  .panel .lab { fill:#9a96a8; font-size:14px; letter-spacing:.08em; } .panel .btxt { fill:#cdd1d8; font-size:13px; }
  .panel .role { fill:#ffa03c; font-size:12px; } .panel .role.big { font-size:18px; fill:#9a96a8; } .panel .screentext { fill:#555; font-size:16px; }
  table { font-size:13px; margin:6px 0 20px; }
  th, td { padding:5px 10px 5px 0; }
  thead th { font-size:12px; letter-spacing:.1em; }
  tbody th { white-space:nowrap; font-weight:700; color:var(--cream); padding-right:14px; }
  .none { color:#555; }
  .cheat h2 { margin:22px 0 8px; font-size:13px; }
  .pads { display:grid; grid-template-columns:repeat(4, minmax(0, 1fr)); gap:6px; max-width:520px; }
  .pad { padding:8px 4px; text-align:center; border-radius:4px; font-weight:700; border:2px solid; }
  .p0 { border-color:#f06446; color:#f06446; } .p1 { border-color:#fabe3c; color:#fabe3c; } .p2 { border-color:#6edcb9; color:#6edcb9; } .p3 { border-color:#aa8cf0; color:#aa8cf0; }
  .tables { overflow-x:auto; }
  @media print {
    @page { size:A4 landscape; margin:9mm; }
    html, body { background:#fff; color:#111; } body { background-image:none; font-size:10.5px; }
    nav.dial { display:none; } .mast svg { display:none; } h1 { font-size:24px; color:#111; } h2 { color:#555; }
    main { max-width:none; padding:0; } .panel { width:60%; margin:0 auto 6px; } .panel .role { fill:#a35a08; } .panel .btn { fill:#fff; stroke:#888; } .panel .btxt { fill:#222; }
    .panel .body { fill:#f1f1f4; stroke:#888; } .panel .tray { fill:#e2e2e8; } .panel .knob { fill:#fff; stroke:#888; } .panel .screen { fill:#ddd; }
    table { font-size:9.5px; } th, td { padding:2px 6px 2px 0; border-color:#bbb; } tbody th { color:#111; } code { background:#ddd; }
    footer { display:none; }
  }
</style>
'''
    body = f'''
  {cheat_panel()}
  <div class="tables"><table>
    <thead><tr><th></th><th>PATTERN</th><th>LAB</th><th>SEND</th></tr></thead>
    <tbody>{rows}</tbody>
  </table></div>
  <div class="cheat">
  <h2>Windows</h2>
  <div class="tables"><table><tbody>{wins}</tbody></table></div>
  <h2>FX pads (tap FX)</h2>
  <div class="pads">{pads}</div>
  </div>
'''
    write("cheatsheet/index.html", page("Cheat sheet · Drumlab", body, 1, "Cheat sheet", tag="CHEAT SHEET", head=css))


def main():
    index(); install(); play(); projects(); guide(); tutorials(); cheatsheet()


if __name__ == "__main__":
    main()
