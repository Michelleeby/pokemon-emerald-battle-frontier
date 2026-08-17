# Pokémon Emerald: Battle Frontier

This project streamlines Pokémon Emerald around its post-game battling. Skip
the main story, build your ideal team, and take on the Battle Frontier immediately.

## Pokémon Lab

![Pokémon Lab Editor](assets/pokemon-lab-editor-tutorial.gif)

The Pokémon Lab is an in-game party editor for quickly creating and refining
Battle Frontier teams. It can create a new Pokémon in an open party slot or
edit an existing party member.

For each Pokémon, the editor supports:

- Species, nature, ability, level, and held item selection
- Individual IV and EV editing with a live stat preview
- Presets for maximum IVs, zero Speed IVs, cleared EVs, and 252/252/4 EV
  spreads
- Move selection limited to moves the species can legally learn through level
  up, TMs and HMs, move tutors, egg moves, or an earlier evolution
- Validation of the completed build before it is saved to the party

Use **L** and **R** to switch between the Build, Stats, and Moves pages. Press
**Start** to save the Pokémon or **B** to cancel. On the Stats page, **Select**
opens the stat presets; on the Moves page, it clears the selected move.

## Building

See [INSTALL.md](INSTALL.md) for toolchain setup and build instructions.

## Releases

Download the BPS patch and checksum manifest from the
[latest release](https://github.com/Michelleeby/pokemon-emerald-battle-frontier/releases/latest).
Apply the patch to a clean US Pokémon Emerald ROM with SHA-1
`f3ae088181bf583e55daf962a92bb46f4f1d07b7`. RomPatcher.js can apply BPS
patches in a browser or from its Node.js command-line interface.

Production ROM SHA-1: `94dd919efb13876a9bfa53a5787f3d294281f3a8`

The original or patched ROM is not distributed by this project.
