## Inspiration
The 3DS can build a Mii from a photo of your face, but the original Wii never could. Making a Mii there means scrolling through hundreds of face shapes, noses and hairstyles with a Wii remote. We wanted to give the Wii that missing feature.

Reverse engineering is also having a moment currently, and computer vision models have now become good enough to run a pipeline like this in real time on a laptop (no GPU).

## What it does
Make Mii! scans your face with a webcam and builds a Mii that matches it, on a real Nintendo Wii.

A computer vision pipeline on the PC finds your facial features and hair, matches their shape and colour to the closest Mii parts (face shape, skin, hair, eyebrows, eyes, nose and mouth) and streams the result to the console over Wi-Fi. A custom Wii homebrew app receives it and draws the Mii live, from the console's original sprite sheets. Pressing **A** on the Wii remote saves the Mii into the Wii's Mii Plaza, where it appears in the Mii Channel like any Mii you made by hand.

## How we built it
**On the PC (Python, `wii-ai`):**
- Captures the user's face from a webcam in real time.
- **MediaPipe Face Landmarker** extracts facial feature landmarks, which give us the geometry of the face, eyes, brows, nose and mouth.
- **MediaPipe hair image segmentation** isolates the hair so we can read its shape and colour.
- **OpenAI CLIP** compares what we extracted against the original Mii sprite assets using image embeddings and picks the closest match for each part.
- Sliders for brightness, contrast, gamma and saturation let the user tune the image for clean separation of skin and hair.
- The chosen Mii configuration is packed into the Wii's 74-byte Mii format and sent over UDP.
- It runs on a laptop's integrated graphics. No dedicated GPU is needed.

**On the Wii (C++, devkitPPC, libogc, GRRLIB, `wii-make-mii`):**
- Wrote a custom homebrew channel that listens for UDP packets on port 8080.
- Each packet is one 74-byte Wii Mii record.
- A reusable renderer (`MiiAvatar`) composites the face layer by layer into a texture and only rebuilds it when something changes.
- Saving writes the record into the Wii's `RFL_DB.dat` on NAND and recalculates its CRC.

## Challenges we ran into
- **Decades-old "documentation"**
    - The Mii format and rendering notes date from around 2010, and parts were incomplete or scattered across wiki pages and archived code.
- **Reverse engineering examples**
    - The only existing example we could find that renders Miis was an old homebrew project. We had to read it, work out which parts still held up on modern GRRLIB and libogc and rebuild the rest.
- **Wrong data from an old library**
    - The Mii library we started with returned garbage values (out-of-range sizes, a 1-bit flag reading 128), so faces came out with tiny eyes, huge mouths and features in the wrong places. We fixed it by writing our own decoder directly from the raw bit layout.
- **Emulator vs real hardware**
    - Dolphin and the real console behaved differently, so we tested on both throughout.
- **Silent crashes**
    - Null pointers from old libraries and other memory leaks had to be traced and fixed.

## Accomplishments that we're proud of
- It works! You can scan a face and see a Mii of yourself appear on a real Wii.
- Building our own custom Wii channel and getting it running on 20-year-old hardware.
- A renderer small enough to reuse in other homebrew: give it 74 bytes and it draws a Mii.
- The whole pipeline runs on an ordinary laptop with integrated graphics, with no GPU, so anyone with a webcam and a Wii can try it.

## What we learned
- **Wii development**
    - The homebrew toolchain, working directly with the console's NAND file system, the GX graphics pipeline and reading bit-packed binary formats.
- **C++ graphics and networking**
    - Off-screen texture compositing, tile-based sprite rendering and non-blocking UDP sockets on embedded hardware.
- **Computer vision**
    - image segmentation, facial landmark recognition and tracking, building an AI pipeline end to end, and using image embeddings (CLIP) to match real photos to cartoon assets.

## What's next for Make Mii!
- Glasses recognition
- Beard and mustache recognition
- Beauty spot recognition
- More advanced hair detection, since hair has the most variety of any Mii part