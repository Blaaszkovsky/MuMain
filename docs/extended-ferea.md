# Ferea in the extended client

With a compatible OpenMU server, open the Warp List with **M**, scroll to the
bottom and select **Ferea**. The destination requires level 400 and costs
50,000 zen. It enters near coordinates 236, 152. The entry appears in the
English, Portuguese and Spanish move lists, as well as the default list.

Ferea uses network map 112 and World113 terrain assets. The client package
must include those assets and be paired with a server whose Ferea map and warp
updates have been applied. This is an extended MuMain destination; the
original Season 6 client does not include it.

The client can display Ferea Knight, Archer, Fighter, General, Lord of Ferea,
and Crystal Orb when its `Data/Monster` directory contains `Monster286.bmd`
through `Monster291.bmd` and their matching OZJ/OZT textures. The three
ordinary enemies appear naturally when the matching OpenMU configuration
update is installed. The General, Lord, and Crystal Orb become encounter
actors when the server's boss encounter configuration is installed.

The General appears at a size comparable to the Lord. Crystal Orbs float above
the ground with a blue glow; a hit briefly changes the glow to orange, and it
fades with the destroyed orb. The Lord and other enemies hold their final death
pose until their bodies disappear. Knight, Archer, and Fighter retain their
native attack, hit and death animations.

These visuals use the existing attack, hit and death messages. They do not
signal boss invulnerability or encounter phases: use the server's encounter
announcements for those rules. The client's blue orb glow indicates a living
orb, not an additional buff or shield. Lord-specific extra skill animations
need an agreed server skill mapping before they can be enabled.
