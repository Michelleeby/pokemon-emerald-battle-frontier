# Complete feature list

This guide describes the gameplay changes from the upstream `pret/pokeemerald`
base, following the project's gameplay commit history.

## Battle Frontier from the beginning

- Begin a new game with a short route to the Battle Frontier, skipping
  Emerald's main-story progression.
- Start with Frontier access, all eight badges, the PokéNav, Frontier Pass,
  running shoes, and a registered Mach Bike.
- Learn to use the PokéNav and Pokémon Lab through a guided introduction.
- Skip the introduction if you prefer. Any nested menus close before control
  returns to the player.
- Opening, naming-screen, ferry, and first-battle transitions have been
  adjusted for the shortened route.
- Choose a starter through a prompt rewritten for the Frontier opening.

## Pokémon Lab

- Create a new Pokémon directly in an empty party slot or edit an existing
  party member.
- Choose the species through a reusable National Pokédex selection and search
  interface.
- Set level, nature, ability slot, held item, IVs, EVs, and moves.
- See calculated stats as you edit IVs and EVs.
- Apply maximum-IV, zero-Speed-IV, cleared-EV, and 252/252/4 EV presets.
- Adjust stats quickly with accelerated directional input.
- Choose moves through a searchable interface.
- Set each move's PP Up count from zero to three, or maximize all eligible
  moves at once. PP edits preserve the amount of PP already spent.
- Choose from the selected species' legal Generation III level-up, TM/HM,
  tutor, egg, and pre-evolution move pools.
- Validate the completed build before saving it to the party.
- Generate species-appropriate gender and preserve the selected ability slot.
- Open the Pokémon Lab from the party menu or PokéNav.

## Team management and interface

- Open both the Pokémon Lab and Pokémon Storage System from the PokéNav.
- Press **L** in the overworld to open the PokéNav once it is available.
- See which stats a nature raises or lowers through distinct colors on the
  Pokémon Summary screen.
- Choose your avatar's appearance independently from the player character's
  gender.
- Present May first in avatar selection and in the title introduction.

## Normal and Hard Frontier challenges

- Choose Normal or Hard difficulty when entering any of the seven facilities:
  Battle Tower, Battle Factory, Battle Dome, Battle Arena, Battle Palace,
  Battle Pike, and Battle Pyramid.
- Face trainers from the strongest Frontier trainer pools from the beginning
  of Hard challenges.
- Encounter Frontier Brains earlier in Hard mode.
- Use Battle Factory rentals with the intended Hard-mode IVs.
- Keep active challenges, streaks, records, and relevant facility progression
  separate for Normal and Hard modes.
- View Normal and Hard results separately from each facility's record computer.
- Retain each facility's Level 50 and Open Level choices and its supported
  battle formats.

## Upstream behavior and development support

- Enable the vanilla bug-fix configuration supplied by `pret/pokeemerald`.
- Provide a dedicated Retroid Pocket Classic build-and-deploy target for local
  development.

This list covers gameplay. Testing infrastructure, release automation, capture
tools, and documentation changes are omitted because they do not change how
the game plays.
