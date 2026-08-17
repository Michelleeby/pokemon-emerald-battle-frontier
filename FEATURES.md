# Complete feature list

This list is derived from the project's gameplay commit history and describes
changes from the upstream `pret/pokeemerald` base.

## Battle Frontier from the beginning

- New games skip Emerald's main-story progression and begin with a short route
  to the Battle Frontier.
- The opening includes the required Frontier access, all eight badges, the
  PokéNav, Frontier Pass, running shoes, and a registered Mach Bike.
- A guided introduction demonstrates the PokéNav and Pokémon Lab before play
  begins in earnest.
- The introduction can be skipped and safely unwinds nested menus before
  returning control to the player.
- Opening, naming-screen, ferry, and first-battle transitions have been adjusted
  for the shortened route.
- The starter-selection prompt has been rewritten for the Frontier-focused
  opening.

## Pokémon Lab

- Create a new Pokémon directly in an empty party slot or edit an existing
  party member.
- Choose the species through a reusable National Pokédex selection and search
  interface.
- Set level, nature, ability slot, held item, IVs, EVs, and moves.
- Preview calculated stats while editing IVs and EVs.
- Apply maximum-IV, zero-Speed-IV, cleared-EV, and 252/252/4 EV presets.
- Adjust stats quickly with accelerated directional input.
- Choose moves through a searchable interface.
- Restrict move choices to the selected species' legal Generation III level-up,
  TM/HM, tutor, egg, and pre-evolution move pools.
- Validate a build before saving it to the party.
- Generate species-appropriate gender and preserve the selected ability slot.
- Open the Pokémon Lab from the party menu or PokéNav.

## Team management and interface

- Open both the Pokémon Lab and Pokémon Storage System from the PokéNav.
- Press **L** in the overworld to open the PokéNav once it is available.
- Show nature-raised and nature-lowered stats with distinct colors on the
  Pokémon Summary screen.
- Choose avatar appearance independently from the player character's gender.
- Present May first in avatar selection and in the title introduction.

## Normal and Hard Frontier challenges

- Choose Normal or Hard difficulty when entering any of the seven facilities:
  Battle Tower, Battle Factory, Battle Dome, Battle Arena, Battle Palace,
  Battle Pike, and Battle Pyramid.
- Use the strongest Frontier trainer pools from the beginning of Hard
  challenges.
- Encounter Frontier Brains earlier in Hard mode.
- Give Battle Factory rentals the intended Hard-mode IVs.
- Keep active challenges, streaks, records, and relevant facility progression
  isolated between Normal and Hard modes.
- View Normal and Hard results separately from each facility's record computer.
- Preserve the facilities' Level 50 and Open Level choices and their supported
  battle formats.

## Upstream behavior and development support

- Enable the vanilla bug-fix configuration supplied by `pret/pokeemerald`.
- Provide a dedicated Retroid Pocket Classic build-and-deploy target for local
  development.

Testing infrastructure, release automation, capture tooling, and documentation
changes are intentionally omitted because they do not change player-facing
gameplay.
