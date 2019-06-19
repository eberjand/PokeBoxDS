## Planned future features

* Ability to manage your item bag and move items to a shared SD storage
  * Gen3 and Gen4 can share an item box, but all other generations will have separate item boxes
  * Key items (and TMs for Gen5 only) cannot be moved
* Multiple box groups
* Ability to trigger trade evolutions if you have the required held item
* Ability to convert Pokémon both forward and backwards between generations without data loss
  * Any Pokémon (except Spiky-Eared Pichu) can be sent to any generation where its species existed.
  * Some data will be temporarily "lost" when withdrawn to a previous generation, but that data will be restored when that same Pokémon is deposited back into PokeBoxDS with the same SD card.
    * Any moves not in the target generation will be deleted
	* Some conversions will require regenerating the personality value
	* Gen1/Gen2 Pokémon always lose DVs/IVs when brought to Gen3+
	* Miscellaneous adjustments to met data, ribbons, Pokéball, etc.
	* Valid nicknames can vary between generations
  * That lost data won't be restored if you transfer a Pokémon with Pal Park or PokéTransfer.
  * Gen1/Gen2 to Gen3+ conversion will imitate PokeTransporter, but without the GB origin marker.
* Support for the DS Generation IV Pokémon games: Diamond, Pearl, Platinum, HeartGold, and SoulSilver
  * DS carts can't be accessed when using an R4, but DSi Mode and sav files will work.
* Support for the DS Generation V Pokémon games: Black, White, Black 2, White 2
* Support for the GB/GBC Pokémon games: Red, Green, Blue, Yellow, Gold, Silver, Crystal
  * Only with `.sav` files on the SD card, such as those created by [Goomba Color](http://www.dwedit.org/gba/goombacolor.php).
  * Unfortunately, there's no way to access GB cartridge saves without specialized hardware
  * It may be possible to access Virtual Console save data when running on a 3DS in DSi mode.
  * These games' boxes will be managed through a similar Gen3-like interface but tweaked to remove gaps: reflow for pick up, insert (instead of swap) for put down, selection becomes linear (and wraps across lines like text), and localized versions use boxes of 20 (not 30) Pokémon each.
* Edit box and group names with a soft keyboard that adjusts to each generation's charset
* Ability to read sav files from the SD card inserted in a Slot-2 cart while running off a Slot-1 card
* Ability to read sav files and PokeBoxDS data from the SD card inserted in a Slot-1 card while running in DSi mode
* Ability to import and export PKM files
* Play music directly from the selected/inserted game

### Maybe / distant future

* A separate build that supports some cheat features like event generating, editing IVs, duplication, etc.
  * My main goal with this project is to reproduce/improve legit-ish features, so this is lower priority to me
  * I'd still maintain a version without these features as a separate build for every release
  * The above-mentioned support for PKM files may suffice for many people
* Local wireless communication between multiple DS systems running PokeBoxDS
* Wi-Fi support for communicating with a PokeBox server
  * I don't know if homebrew can use WPA2 on DS Lite. If not, it'd be a lot less useful.
* Support for moving Pokémon between PokeBoxDS and PKSM save data
* Ability to view party and day care Pokémon
  * Party can be modified if you saved in a Pokémon Center
  * Day care Pokémon will be read-only
