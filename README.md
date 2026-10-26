# Make Mii!

<div align="center">
  <img src="https://raw.githubusercontent.com/Make-Mii/wii-ai/main/assets/logo.png" alt="Make Mii Logo" width="300" />

  <p><strong>Real-time facial capture to playable Nintendo Wii Mii avatars</strong></p>

  <p>
    <a href="https://hacknotts.com">Built for HackNotts '26</a>
  </p>
  <p>
    <img src="https://img.shields.io/badge/🏆_Best_AI%2FML_Project-HackNotts_'26-gold?style=for-the-badge" alt="Best AI/ML Project" />
    <img src="https://img.shields.io/badge/🥈_Runner--up-Telluvian_Best_Use_of_Multiple_AI_Systems-silver?style=for-the-badge" alt="Telluvian Runner-up" />
  </p>
</div>

## Overview

Make Mii! scans your face and turns it into a Mii on a real Wii. The project is split into two repos:

| Repo | Role |
|---|---|
| [**wii-ai**](https://github.com/Make-Mii/wii-ai) | Scans the user's face, generates the Mii and sends it to the console |
| **wii-make-mii** (this repo) | The Wii homebrew app: receives Mii data, renders it live and saves it to the Mii Plaza |

This repo exists to build the `.dol` that `wii-ai` talks to

## Controls

| Button | Action |
|---|---|
| **A** | Save the current Mii to the Mii Plaza |
| **B** | Freeze / resume the live stream |
| **HOME** | Exit |

## Build

Requires [devkitPro](https://devkitpro.org) with devkitPPC, libogc and [GRRLIB](https://github.com/GRRLIB/GRRLIB)

```bash
make
```

This produces `wii-make-mii.dol`. Copy it to `apps/wii-make-mii/boot.dol` on your SD card/USB and launch it from the Homebrew Channel

## Running the full pipeline

1. Put the Wii and your PC on the same network and launch Make Mii! on the Wii. The top bar shows the Wii's IP address
2. Follow the [wii-ai](https://github.com/Make-Mii/wii-ai) instructions and point it at that IP, port **8080**
3. Scan your face. The Mii appears live on the Wii
4. Press **A** on the Wii remote to save it to the Mii Plaza

## Protocol

- The Wii listens for **UDP datagrams on port 8080**
- Each packet is exactly one **74-byte big-endian Wii Mii record**, the same layout as an entry in `RFL_DB.dat` ([format on WiiBrew](https://wiibrew.org/wiki/Mii_data))
- Packets of any other size are ignored

## Testing without the AI

`mii-sender/mii-sender.py` is a small test script that sends a single `.mii` file to the Wii, so you can check the console side works without running `wii-ai`

```bash
# edit WII_IP and mi_path at the top of the script, then:
python mii-sender/mii-sender.py
```

## Warning

Saving writes to `/shared2/menu/FaceLib/RFL_DB.dat` on the Wii's NAND. Back up your NAND first
