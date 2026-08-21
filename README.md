>## 📌 Important
> We recommend [CrankBoy](https://github.com/CrankBoyHQ/crankboy-app), an enhanced version of PlayGB with excellent performance, advanced features, and improved ROMs compatibility.
> 
> [Get CrankBoy](https://github.com/CrankBoyHQ/crankboy-app)
>
> You can still use PlayGB as an alternative, for example, to test a specific ROM.

<p>
<img src="assets/playgb-logo-2x.png?raw=true" width="200">
</p>

## PlayGB (Optimized Fork)

A high-performance Game Boy emulator for the Panic Playdate console, based on [Peanut-GB](https://github.com/deltabeard/Peanut-GB) (a header-only C Game Boy emulator by [deltabeard](https://github.com/deltabeard)) and [minigb_apu](https://github.com/baines/MiniGBS).

This fork features a comprehensive, end-to-end performance overhaul:
- **Direct 4KB Page Table Memory Bus**: Eliminates multi-branch memory decoding with 1-cycle pointer lookups.
- **5.0x Faster Parallel Framebuffer Dithering**: 32-bit word-packed spatial dithering preserving 100% bit-exact monochrome output.
- **Division-Free Audio Synthesis**: Replaced runtime divisions with shift tables and hoisted gain scaling.
- **Streaming L1-Cache Friendly PPU Renderer**: Left-to-right sequential tile rendering.

## Installing

<a href="https://github.com/risolvipro/PlayGB/releases/latest"><img src="assets/playdate-badge-download.png?raw=true" width="200"></a>

* Download the zip from the [latest release](https://github.com/risolvipro/PlayGB/releases/latest).
* Copy the pdx through the [Web sideload](https://play.date/account/sideload/) or USB.
* Launch the app.
* Connect Playdate to a computer, press and hold `LEFT` + `MENU` + `LOCK` at the same time in the homescreen. Or go to Settings > System > Reboot to Data Disk.
* Place the ROMs in the app data folder, the folder name depends on the sideload method.
    * For Web sideload: `/Data/user.*.com.risolvipro.playgb/games/`
    * For USB: `/Data/com.risolvipro.playgb/games/`
* Filenames must end with `.gb` or `.gbc`

## Notes

* Use the crank to press Start or Select.
* To save a game you have to use the save option inside that game. A sav file is automatically created when changing ROMs or quitting the app. After a crash, a new `(recovery).sav` file is created. Save files are stored in `/Data/*.playgb/saves/`
* Audio is supported with high efficiency. You can optionally enable it from the library screen.

## AI Disclosure & Attribution

This fork was developed and performance-optimized by **Radu Gabriel** in collaboration with AI models **Gemini 3.7 Flash** and **Claude Sonnet 4.6**. The AI was utilized for deep bottleneck profiling, architectural refactoring (page tables, branch prediction hints, assembly-level loop optimizations), parallel 1-bit LUT dithering vectorization, and unit test benchmarking.