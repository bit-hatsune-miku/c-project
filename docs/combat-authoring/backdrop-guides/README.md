# Backdrop Floor Cut Guides

These guides are Photoshop-ready overlays for the current battle camera.

How to read them:

- red = the in-game floor is covering the backdrop here
- amber = blend the real photo ground through this band so the handoff feels intentional
- magenta = current size-0 floor diagnostic range from the repo; useful for spotting floating/cut-in-half stages, not the target
- white rectangle = what the game actually sees from the larger source image

Files:

- `screen_template.png`: fixed full-screen backdrops
- `parallax_template.png`: oversized parallax source art
- `panorama_template.png`: 2:1 panorama source art
- `skybox_*.png`: one guide for each cube face
- `guide_notes.json`: numeric values used to generate the guides

Regenerate with:

```bash
python3 scripts/generate_floor_cut_guides.py
```
