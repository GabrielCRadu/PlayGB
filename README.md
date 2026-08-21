<p>
<img src="assets/playgb-logo-2x.png?raw=true" width="200">
</p>

## PlayGB (Optimized Fork)

A Game Boy emulator for the Panic Playdate console, based on [Peanut-GB](https://github.com/deltabeard/Peanut-GB) (a header-only C Game Boy emulator by [deltabeard](https://github.com/deltabeard)) and [minigb_apu](https://github.com/baines/MiniGBS).

This fork focuses on architectural refactoring and performance optimizations designed to reduce CPU load:
- **Direct 4KB Page Table Memory Bus**: Replaces multi-case switch decoding with direct pointer lookups.
- **Parallel Framebuffer Dithering**: 32-bit word-packed spatial dithering while preserving 100% bit-exact monochrome output.
- **Division-Free Audio Synthesis**: Replaces runtime hardware divisions with shift operations and hoisted gain scaling.
- **Sequential PPU Renderer**: Left-to-right tile streaming to improve L1 cache locality.

*For full technical details, synthetic benchmarks, and changelogs, see the [Releases tab](https://github.com/GabrielCRadu/PlayGB/releases) and [CHANGELOG.md](CHANGELOG.md).*

## Installing

<a href="https://github.com/GabrielCRadu/PlayGB/releases/latest"><img src="assets/playdate-badge-download.png?raw=true" width="200"></a>

* Download the zip from the [latest release](https://github.com/GabrielCRadu/PlayGB/releases/latest).
* Copy the pdx through [Web sideload](https://play.date/account/sideload/) or USB.
* Launch the app.
* Connect Playdate to a computer, press and hold `LEFT` + `MENU` + `LOCK` at the same time in the homescreen (or go to Settings > System > Reboot to Data Disk).
* Place ROMs in the app data folder:
    * For Web sideload: `/Data/user.*.com.radugabriel.playgb/games/`
    * For USB: `/Data/com.radugabriel.playgb/games/`
* Filenames must end with `.gb` or `.gbc`.

## Notes

* Use the crank to press Start or Select.
* To save a game, use the in-game save feature. A sav file is automatically created when changing ROMs or quitting. After an error, a `(recovery).sav` file is generated. Save files are stored in `/Data/*.playgb/saves/`.
* Audio support is included and can be toggled from the library screen.

## AI Disclosure & Attribution

This fork was developed and performance-refactored by **Radu Gabriel** in collaboration with AI models **Gemini 3.7 Flash** and **Claude Sonnet 4.6**. The AI was utilized for profiling, architectural refactoring (page tables, branch hints, loop restructuring), parallel 1-bit LUT dithering, and automated unit test benchmarking.
