# Zelda's Birthday ROM Fixer

This ad hoc utility was thrown together to quickly fix miscellaneous files from an ancient Zelda 64 mod titled Zelda's Birthday so it can be played on a wider variety of emulators, as well as on real Nintendo hardware.

## Types of fixes it applies

### Scene and room file fixes

#### General (systemwide)

It fixes scene and room files by recursively stepping through all actor layouts and deleting references to nonexistent overlay IDs, which are very error-prone.

#### House of Skulltula

~~Indoor lighting mode was causing a crash. The scene's lighting mode was set to outdoor lighting to remedy the problem.~~

Update: This fix is no longer necessary?

#### Eagle Labyrinth

One feature that made Zelda's Birthday such a historical milestone in our community was the inclusion of a custom dungeon. Because this dungeon predates all software for importing custom models into the game, its collision data was cleverly crafted by stitching multiple copies of the Fire Temple's entrance room together on a large grid. [Unused staircases hidden beneath the floors of every room](img/eagle-staircases.gif) contribute to an excruciatingly slow load time of approximately 20 seconds. Modern tools were used to create [a new, optimized collision mesh](img/eagle-optimized.png), significantly reducing the load time. Players are no longer left wondering whether the game has frozen, as it takes only a second or two to load into the scene.

Additionally, one room in this dungeon had an actor dependency that was causing a crash. Once it was narrowed down the the ladder actor specifically, a graphically and functionally equivalent replacement was created.

### Actor overlay file fixes

The original author's game code was preserved when possible. In cases where they introduced unstable changes, the modified code was disassembled and carefully inspected to figure out its intent, and substituted with functionally equivalent code that doesn't crash the game.

#### En_Sa (Saria)

Saria's actor is hard-coded to play a cutscene when you first enter Kokiri Forest. The author of Zelda's Birthday turned this behavior off by zeroing out some opcodes. The original changes render the game unplayable on hardware and most emulators, so a new solution was engineered. The new solution involves having [this function](https://github.com/zeldaret/oot/blob/e37b9934837ec96304a3ef9576d8d283cfa0f7bb/src/overlays/actors/ovl_En_Sa/z_en_sa.c#L383) return `3` in cases where it would otherwise return `4`, effectively disabling the cutscene without breaking anything.

### DMA table fixes (`dmadata`)

Many files are not referenced by the DMA table because they were either resized, relocated, or not originally part of the game. The absence of entries for these files does not usually cause issues, but there are cases where it may, and it prevents the game from having its filesystem compressed.

## Credits

- [@z64me](https://github.com/z64me) - this program, finding bugs, fixing bugs
- [@Zeldaboy14](https://github.com/Zeldaboy14) - finding bugs, fixing bugs
- [@ThreePendants](https://github.com/ThreePendants) - finding bugs, fixing bugs
- [Edwin Bruce Shankle IV](https://www.youtube.com/c/BruceShankle) - finding bugs
- [@AriaHiro64](https://github.com/AriaHiro64) - finding bugs
