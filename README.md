# PokeBoxDS

## About

PokeBoxDS is a Pokémon box manager for GBA Pokémon games that runs on DS homebrew.

**This project is an early work in progress.** It already has some useful functionality, but many of the most important planned features are missing.

PokeBoxDS is not in any way affiliated with or endorsed by Nintendo, The Pokémon Company, or GAME FREAK.

The PokeBoxDS project does not endorse video game piracy. The code and data included with this project is 100% free software (GPLv3 or compatible) and does not include any non-free dumped game assets. This project is intended to be used with legitimate game cartridges and legal dumps of those cartridges.

## Installation

Disclaimer: This project and its author are not in any way affiliated with the stores, products, and other web sites linked from this document.

### DS or DS Lite

1. If you don't already have a DS flash card that can run homebrew, buy one and set it up.
    1. I recommend [R4i Gold 3DS Plus](https://www.nds-card.com/ProShow.asp?ProID=575), which also includes a USB microSD reader
    2. Get a microSD card. Any size is fine; even 1GB is way more than we need. Large cards above 128GB (SDXC) need to be reformatted as FAT32.
    3. Download [Wood Kernel for R4i](https://mega.nz/#!rpkhRCLB!yQdCQGOMfm3LZaf6DYOho60w5M530CZAKWu6uvQX_Eo) ([Alternate link](http://r4ids.cn/r4i-download-e.htm)).
    4. If you're running Windows, install [7-Zip](https://www.7-zip.org/). On other platforms, install `unrar` or anything else that can extract RAR files.
    5. Extract the files from `Wood_R4iGold_V1.64.rar`
    6. Move `__rpg` and `_DS_MENU.DAT` to the SD card root
2. Download the latest `PokeBoxDS.nds` and move it to the SD card
3. Insert the microSD card firmly in your R4 and insert the R4 card into your DS.

For some older flash cards, you may have to run a DLDI patcher. Find and follow instructions specific to your flash card. R4 and AceKard users can safely ignore this because those cards support auto-patching.

### DSi or 3DS

DSi doesn't have a GBA slot, so it's not very useful for the current version of PokeBoxDS. It can still load sav files from the SD card.

R4i 3DS works on DSi, but some flash cards have limited DSi compatibility. No DS flash cards seem to work on the latest stock 3DS firmware.

You can avoid buying a flash card entirely by installing [DSi CFW](https://dsi.cfw.guide/) or [3DS CFW](https://3ds.hacks.guide/). Install [TWiLightMenu](https://github.com/DS-Homebrew/TWiLightMenu/releases) and use it to load `PokeBox.nds` from your SD card.

## Usage

Controls in box view:

* D-Pad: Move the cursor
* A: Tap to pick up or put down a Pokemon. Hold while moving the D-Pad to make a multi-selection for moving multiple Pokemon at once.
* X: Swap box groups
* L/R: Switch boxes
* Start, Select, and Y currently do nothing

Controls in menus:

* D-Pad Up/Down: Move the cursor to the previous or next item
* D-Pad Left/Right: Jump back or forward a whole page
* A: Select a menu item
* B: Go back to the previous menu
* L/R: Scroll long filenames

Insert a compatible GBA cartridge before selecting `Slot-2 GBA Cartridge` from the main menu. Don't remove the cartridge without first going back to the top menu. Alternatively, place a `.gba` file with its corresponding `.sav` on your SD card then select it from the file browser.

## Features

* Supported games:
  * Gen3: Pokémon Ruby, Sapphire, FireRed, LeafGreen, and Emerald
  * All langauges: Japanese, English, Spanish, French, German, Italian
* Move Pokémon between the PC boxes in your games and a large shared Pokémon storage on the SD card
* Withdraw Pokémon to a different game; you can move a huge number of your Pokémon across save files very quickly
* Free up space on your in-game PC
* View more detailed data from the Pokémon you've caught than is available in-game
  * Personality, EV (Effort Values), and IV (Individual Values) data
  * Look inside Pokémon Eggs before they hatch

### Comparison to official Pokémon software

PokeBoxDS is similar in goal to the GameCube software *Pokémon Box: Ruby & Sapphire*, but has some additional features, requires much less hardware, and doesn't require buying an expensive secondhand rare disc. Unlike *Pokémon Box: Ruby & Sapphire*, PokeBoxDS does not place withdraw restrictions based on factors like how many Pokémon are marked as owned in your Pokédex.

PokeBoxDS can replace some uses for trading with yourself between multiple Pokémon games without the need for multiple Game Boy Advance systems or a link cable. It's also much faster: you can move hundreds of Pokémon around in the time it'd take to complete just a couple trades.

## Building

After [installing devkitPro](https://devkitpro.org/wiki/Getting_Started) with the `nds-dev` group:

```
git clone https://github.com/eberjand/PokeBoxDS.git
cd PokeBoxDS
make
```
