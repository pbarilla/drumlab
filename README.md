# Drumlab

A Microtonic drum machine for the FM-1 that can send kits, patterns, chains, etc to Microtonic enabled Pocket Operators

<p>
<img src="site/img/playing.png" width="32%"> <img src="site/img/snd.png" width="32%"> <img src="site/img/send.png" width="32%">
<img src="site/img/kits.png" width="32%"> <img src="site/img/picker.png" width="32%"> <img src="site/img/menu.png" width="32%">
</p>

## What is it

16 sounds, 16 patterns, 4 lanes, all the PO-32's tricks, Brothers Link ready, and a kitchy retro-futuristic UI that resembles a science lab. Craft the perfect kit, follow along with the experiments, and then send it all to your Pocket Operator!

Dont own a PO-32? It's still a pretty good drum machine.

## Links

- [Play online](https://ihavenocluewhatimdoing.com/drumlab/play/)
- [Install](https://ihavenocluewhatimdoing.com/drumlab/install.html)
- [Manual](https://ihavenocluewhatimdoing.com/drumlab/guide/)
- [Tutorials](https://ihavenocluewhatimdoing.com/drumlab/manual/)
- [Cheat sheet](https://ihavenocluewhatimdoing.com/drumlab/cheatsheet/)
- [Projects](https://ihavenocluewhatimdoing.com/drumlab/projects/)

## Microtonic in, and out

The Projects page can bring Microtonic into Drumlab. Drop `.mtpreset` presets, `.mtdrum` drum patches, or a pattern dragged out of Microtonic as MIDI. Pick which sounds and which patch side (A or B) they land on, read the short note on what changes and what is left out, then add them to a project from the FM-1, a sample, or a file of your own. With no FM-1 attached you get a `.drumlab` to restore later; with one, it only writes when you press the button, and it names the slot. You can also export: your kit as Microtonic presets and drum patches, and kits or patterns as small Drumlab files you can swap with friends and import again.

## Controls

|                       | PATTERN                                  | LAB                      | SEND                |
| --------------------- | ---------------------------------------- | ------------------------ | ------------------- |
| `HOME`                | next view, hold: projects                | same                     | same                |
| `PRESETS`             | sound                                    | sound                    |                     |
| `SELECT`              | pattern                                  |                          |                     |
| `ALGORITHM`           |                                          | page                     |                     |
| `KNOB 1`-`4`          | tempo, swing, pitch, morph               | the page's four settings |                     |
| `REC`                 | tap: WRITE, hold while playing: punch in |                          |                     |
| `ENV` / `ARP` / `SEL` | accent / mute, solo / chain              |                          |                     |
| `FX`                  | hold: effects, tap: latch                |                          |                     |
| `EDIT`                | hold: clear                              | hold: fine knobs         |                     |
| `SEL`                 |                                          | patch A / B              |                     |
| `OCT+`                | copy the pattern                         | kits                     | yes (in the window) |
| `PLAY`                | play / stop                              | loop the sound           | send                |
| `GLO`                 | help                                     | help                     | help                |
| `GLO` + `HOME`        | menu                                     | menu                     | menu                |

## The three brothers

[Chipbox](https://github.com/pbarilla/chipbox) (chiptunes), [Smileyface](https://ihavenocluewhatimdoing.com/smileyface/) (acid) and Drumlab. Over Bluetooth one of them leads and the others follow (GLO + HOME, BLUETOOTH): tempo, start and stop, a sidechain from the leader's kicks, patterns, punch-in effects and key. Each can also send and follow the standard MIDI clock over USB (MIDI CLOCK).

## Brothers Link (Bluetooth sync between the brothers)

Brothers Link is how the three play together over Bluetooth: Bluetooth Low Energy underneath, but not standard BLE-MIDI. No cables, no pairing: one FM-1 leads, the others follow. Any of the three can lead.

- Tempo, play and stop follow the leader. Followers start at the next bar.
- Sidechain: followers duck on the leader's kicks. Drumlab sends its first lane's, and ducks the whole mix when it follows.
- Patterns: when the leader changes pattern, followers change at the end of their loop. A pattern with nothing in it is skipped. Swing comes along.
- Punch-in FX: hold pads 1 to 8 on the leader and the followers do it too.
- Key: Chipbox and Smileyface send theirs. Drumlab has no notes, so it has no Key row.

Hit `GLO` + `HOME`, pick BLUETOOTH, set SYNC to LEAD on one FM-1 and FOLLOW on the others (`OCT+` changes it). The switches below are off until you turn them on. SYNC is always OFF after power-up, and set it to OFF before you install a firmware.

Writing firmware for the FM-1? Add Brothers Link to yours: [the spec](brothers-link/BROTHERS-LINK.md), [a version for your coding agent](brothers-link/BROTHERS-LINK-FOR-AGENTS.md) and [the eight punch-in effects, with recipes](brothers-link/PUNCH-IN-FX.md).

### Why is it called Brothers Link?

Toyota refers to their trio of 80s and 90s sports cars as the Three Brothers. The Supra, Celica, and MR2. While developing these firmwares, I referred to each of them as one of these codewords. Chipbox was MR2, Smileyface was Celica, and Drumlab was Supra. Collectively, I call my three firmwares the Three Brothers. I also personally own an MR2 with a dreamcast installed in it, but that's another story...

### Do I need to call it Brothers Link?

Absolutely not. If you do choose to use the novel method for connecting 2+ FM-1s together over Bluetooth, call it whatever you like!

## Licence

GPL-3.0-only, see `LICENSING.md`. Built on [Chipbox](https://github.com/pbarilla/chipbox), which is built on [Dinghy](https://github.com/zvenson/dinghy), which is built on
[Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (Hügelton Instruments) with the changes of
[SLOOP](https://github.com/isod89/sloop-fm1) (isod89). PO-32 and Pocket Operator belong to Teenage Engineering.

You can also try Drumlab in the browser, no flashing needed, in the free community [FM-1 Emulator](https://fm1-emulator.atst.tools/).
