# Zelda's Birthday ROM Fixer

This ad hoc utility was thrown together to quickly fix miscellaneous files from an ancient Zelda 64 mod titled Zelda's Birthday so it can be played on a wider variety of emulators, as well as on real Nintendo hardware.

## Types of fixes it applies

### Scene and room file fixes

It fixes scene and room files by recursively stepping through all actor layouts and deleting references to nonexistent overlay IDs, which are very error-prone.

## Actor overlay file fixes

### En_Sa (Saria)

Saria's actor is hard-coded to play a cutscene when you first enter Kokiri Forest. The author of Zelda's Birthday turned this behavior off by zeroing out some opcodes. The original changes render the game unplayable on hardware and most emulators, so a new solution was engineered. The new solution involves having [this function](https://github.com/zeldaret/oot/blob/e37b9934837ec96304a3ef9576d8d283cfa0f7bb/src/overlays/actors/ovl_En_Sa/z_en_sa.c#L383) return `3` in cases where it would otherwise return `4`, effectively disabling the cutscene without breaking anything.
