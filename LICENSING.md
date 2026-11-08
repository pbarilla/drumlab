# Licensing

Drumlab is free software under the GNU General Public License, version 3 only (`GPL-3.0-only`, full text in
`LICENSE`). You may use, study, change and share it. If you give someone a firmware built from it, give them its
complete source under the same licence too.

## Who made what

|                                                                                                                                                                                           |                                                                                                                                                                                                   |
| ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| The platform: `firmware/hal/`, `firmware/loader/`, `crt0.S`, `app.ld`, `firmware/src/` (`usb.c`, `ota.c`, `recovery.c`, `lcd.c`, `gfx.c`, `libc.c`, `bootguard.h`, `uac_fir.h`), `tools/` | Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments ([Felucca](https://github.com/hugelton/Felucca)), with the changes of [SLOOP](https://github.com/isod89/sloop-fm1) (isod89) |
| The browser installer `site/js/fm1ota.js` and `site/js/fm1pkg.js` (unchanged), and what `site/js/install.js` follows | Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments ([Felucca](https://github.com/hugelton/Felucca), GPL-3.0-only) |
| The wiring `platform.c`, the host mode `tests/host.c`, the docs, from Dinghy                                                                                                              | zvenson                                                                                                                                                                                           |
| Drumlab itself: `lab.c`, `labcore.c`, `labui.c`, `labdraw.c`, `labgen.c`, `labdemo.c`, `labio.c`, `labproj.c`, `drum.c`, `po32tx.c`, `bfx.c`, `kits/`, `tests/` scripts, `web/`, `site/` (the app began as Chipbox's, by pbarilla) | pbarilla |

## Third-party material

| What                                                                                                                                                                 | Licence     | Where                                           |
| -------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------- | ----------------------------------------------- |
| Terminus font 8x16 (ter-u16n), rendered into `firmware/gen/felucca_font.h`                                                                                           | SIL OFL 1.1 | `assets/fonts/Terminus-LICENSE.txt`             |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0  | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |
| Special Elite (the tutorials' typewriter), `site/fonts/` | Apache-2.0 | `site/fonts/SpecialElite-LICENSE.txt` |
| Caveat (the tutorials' handwriting) and Press Start 2P, `site/fonts/` | SIL OFL 1.1 | `site/fonts/Caveat-OFL.txt`, `site/fonts/OFL.txt` |
| The Bluetooth radio and link layer, `firmware/ble/` (copies, see its `README.md`): the stack from [fm1-base](https://github.com/charlesvestal/fm1-base) (Henri David and Charles Vestal) and the radio drivers from [felucca-bsp](https://github.com/hugelton/felucca-bsp) | GPL-3.0-only OR MPL-2.0 (used under the GPL); MPL-2.0 and Apache-2.0 | `firmware/ble/LICENSES/` |

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments; "M-VAVE" and "FM-1" belong to their
owners. "PO-32", "PO-35" and "Pocket Operator" belong to Teenage Engineering, and "Microtonic" to Sonic Charge. Drumlab is not
made or endorsed by any of them. The PO-32 transfer format was read from [libpo](https://github.com/ericlewis/libpo) (MIT), which is
used only to check our output in `tests/` and is not part of this tree.
