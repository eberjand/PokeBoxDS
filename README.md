# PokeBoxDS

## About

PokeBoxDS is a Pokémon box manager for GBA Pokémon games that runs on DS homebrew.

**This project is an early work in progress.** It already has some useful functionality, but many of the most important planned features are missing.

PokeBoxDS is not in any way affiliated with or endorsed by Nintendo, The Pokémon Company, or GAME FREAK.

We do not endorse video game piracy. The code and data included with this project is 100% free software (GPLv3 or compatible) and does not include any non-free dumped game assets. You may freely use this software with your legitimate game cartridges and legal dumps of those cartridges.

## Installation

### DS or DS Lite

1. If you don't already have a DS flash card that can run homebrew, buy one and set it up.
    1. We recommend [R4i Gold 3DS Plus](https://www.nds-card.com/ProShow.asp?ProID=575), which also includes a USB microSD reader
    2. Get a microSD card. Any size is fine; even 1GB is way more than we need. Large cards above 128GB (SDXC) need to be reformatted as FAT32.
    3. Download [Wood Kernel for R4i](https://mega.nz/#!rpkhRCLB!yQdCQGOMfm3LZaf6DYOho60w5M530CZAKWu6uvQX_Eo) ([Alternate link](http://r4ids.cn/r4i-download-e.htm)).
    4. If you're running Windows, install [7-Zip](https://www.7-zip.org/). On other platforms, install `unrar` or anything else that can extract RAR files.
    5. Extract the files from `Wood_R4iGold_V1.64.rar`
    6. Move `__rpg` and `_DS_MENU.DAT` to the SD card root
2. Download the latest `PokeBoxDS.nds` and move it to the SD card
3. Insert the microSD card firmly in your R4 and insert the R4 card into your DS.

For some older flash cards, you may have to run a DLDI patcher. Find and follow instructions specific to your flash card. R4 and AceKard users can safely ignore this because those cards support auto-patching.

### DSi or 3DS

DSi doesn't have a GBA slot, so it's not very useful for PokeBoxDS. It can still load sav files from the SD card.

R4i 3DS works on DSi, but some flash cards have limited DSi compatibility. No DS flash cards seem to work on the latest stock 3DS firmware.

You can avoid buying a flash card entirely by installing [DSi CFW](https://dsi.cfw.guide/) or [3DS CFW](https://3ds.hacks.guide/). Install [TWiLightMenu](https://github.com/DS-Homebrew/TWiLightMenu/releases) and use it to load `PokeBox.nds` from your SD card.

## Usage

Controls:

* D-Pad: Move the cursor (in vertical lists, Left/Right jumps a whole page)
* A: Open the selected item
* B: Go back to the previous menu
* L/R: Switch boxes (in Box view) or scroll long filenames (in the file browser)
* Start, Select, X, and Y currently do nothing

Insert a compatible GBA cartridge before selecting `Slot-2 GBA Cartridge` from the main menu. Don't remove the cartridge without first going back to the top menu.

## Features

* Supported games:
  * Gen3: Pokémon Ruby, Sapphire, FireRed, LeafGreen, and Emerald
  * All langauges: Japanese, English, Spanish, French, German, Italian
* View more detailed data from the Pokémon in your PC than is available in-game
  * Personality, EV (Effort Values), and IV (Individual Values) data
  * Look inside Pokémon Eggs before they hatch

### Planned future features

* Move Pokémon between your game's save data and a your SD card
  * and a ton of subfeatures within this that aren't worth detailing yet like Box Groups, generation conversion, etc.
* Support for the GB/GBC Pokémon games: Red, Green, Blue, Yellow, Gold, Silver, Crystal
  * Only with `.sav` files on the SD card, such as those created by [Goomba Color](http://www.dwedit.org/gba/goombacolor.php).
  * Unfortunately, there's no way to access GB cartridge saves without specialized hardware
* Support for the DS Pokémon games: Diamond, Pearl, Platinum, Black, White, Black 2, White 2
  * Can't access DS cartridges while running PokeBoxDS from a Slot-1 flash card, but alternatives can work:
    * Reading from sav files on the SD card instead of cartridges
    * Slot-2 cards that support DS mode, like M3 Perfect
	* Run on a DSi and use its SD card slot
* Ability to read sav files from the SD card inserted in a Slot-2 cart while running off a Slot-1 card

## Comparison to official Pokémon software

Some official utilities have limitations that are solved by using PokeBoxDS instead:

* [Pokémon Box Ruby & Sapphire](https://bulbapedia.bulbagarden.net/wiki/Pok%C3%A9mon_Box_Ruby_%26_Sapphire)
  * Had limited availability and may be expensive in the secondhand market
  * Requires a GameCube or backwards-compatible Wii
  * Requires a GCN-GBA Game Link Cable
  * Requires an original Game Boy Advance or Game Boy Advance SP
    * Game Boy Micro and the DS family don't support the needed link cable
  * Consumes 59 blocks in a GameCube memory card, which completely fills small cards

Planned support for additonal Pokémon generations should also be able to replace Oak's Laboratory in Pokémon Stadium 1-2 and [My Pokémon Ranch](https://bulbapedia.bulbagarden.net/wiki/My_Pok%C3%A9mon_Ranch).

## Building

After [installing devkitPro](https://devkitpro.org/wiki/Getting_Started) with the `nds-dev` group:

```
git clone https://github.com/eberjand/PokeBoxDS.git
cd PokeBoxDS
make
```
