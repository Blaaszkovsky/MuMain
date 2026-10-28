# Move-list data

The four Season 6 move-list files must match the server's warp catalog. Add the
reviewed Ferea or Crywolf entries to a **staging copy**, then restart that client
to load the new list. Do not replace assets beneath running clients.

```powershell
python -B tools/MoveReq/add_crywolf.py --root <staging-client>/Data/Local
python -B -m unittest discover -s tools/MoveReq -p "test_*.py" -v
```

Crywolf uses warp 26, level 190 and 10,000 Zen, targeting gate 118. The character must
still satisfy the authoritative server catalog; being able to enter Crywolf is
separate from the higher altar-contract level requirement. The patch preserves
every existing move-list record and refuses a conflicting index or occupied
next slot. Running it again leaves identical bytes.

Keep the original files and package SHA256 manifests. This format has a count
and fixed-capacity encrypted records, with no checksum trailer. Verify an actual
M-list Crywolf entry, ordinary warp, received map 34 and NPCs on the matching
server before promoting a package.
