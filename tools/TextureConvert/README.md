# Offline texture normalization

Use this tool to prepare individual images for MuMain's existing JPG/TGA model
texture loader. It writes an uncompressed 32-bit, bottom-first OZT and a JSON
manifest into a **new** directory. It never modifies the source, models, runtime,
or configuration. Keep the original source: output preserves decoded RGBA pixels,
not image metadata or the original compression stream.

Requires Python 3.12+ and the optional offline Pillow dependency:

```powershell
python -m pip install -r tools/TextureConvert/requirements.txt
python tools/TextureConvert/convert_texture.py source.OZP staging/new-texture
python tools/TextureConvert/convert_texture.py decrypted.dds staging/new-dds
python tools/TextureConvert/convert_texture.py decrypted.ozd staging/new-dds --format dds
python -m unittest discover -s tools/TextureConvert -v
```

Input supports PNG, TGA, already decrypted DDS, OZP (4-byte prefix), OZT (4-byte
prefix), and OZJ (24-byte prefix; JPEG or the observed mislabeled TGA variant).
**Encrypted OZD is not supported**: use a separately verified decryption stage,
retain its source/payload hashes, then supply DDS bytes explicitly. OZG, Flash/GFx
and `.ein` effect definitions require client functionality and are outside this
image conversion tool.

DDS accepts a single DXT1/DXT3/DXT5 or tightly packed 24/32-bit RGB surface; mipmaps,
cube/volume maps, extra/truncated surfaces and other compression are refused.
Images over MuMain's 1024x1024 texture limit, animated images, and higher precision
PNG are refused rather than resized, flattened, or quantized.

The output filename is the source stem plus `.OZT`. The manifest records source,
output and decoded RGBA hashes, dimensions, Pillow version, and the required
logical `.tga` reference. The caller must explicitly map that reference into the
model or UI integration and choose the intended texture folder. A `.dds`, `.png`,
or mislabeled `.jpg` model reference will not automatically select the new OZT.
Check for destination collisions before assembling an asset tree. This tool does
not choose between assets sharing the same stem.

Existing output directories are refused. If a disk write fails, keep the original
source and inspect/remove only that incomplete staging directory before retrying.
Successful conversion verifies image layout, not materials, blending, effects,
runtime loading, rendering, or original-client compatibility.
