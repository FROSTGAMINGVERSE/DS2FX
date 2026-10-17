# Grimdark Hollow and Human frame assets

Original DS2FX-owned frame art, generated with the built-in image generation tool from the approved concepts in local/design/hud-concepts-20261002. No original game artwork is baked into either atlas. 1254x1254 RGBA PNGs, transparent interiors and backdrop; immutable SHA identities are checked by remastered_skin.h.

The renderer selects measured source rectangles without editing the atlas files. Both skins share the same destination geometry. Original covenant and effects pixels load from the separate local Assets cache. Effects fit visible alpha bounds while preserving aspect ratio and native tint/opacity.

## Exact shared generation prompt

Use case: precise-object-edit. Asset type: production 2D game HUD sprite atlas. Input image is the approved style reference. Extract and faithfully recreate its EXACT grimdark material, scars, surface texture, silhouette character and edge lighting as FOUR SEPARATE EMPTY FRAME components, neatly separated, on TRUE TRANSPARENT background. No opaque backdrop, no checkerboard baked into pixels, no text, no icons, no red or green fills, no symbol, no covenant artwork. Square 1536x1536 atlas. Layout: top row a single perfectly circular covenant ring occupying x100..500 and y100..500, with its entire inner disk transparent. Second row: a long horizontal empty health rail occupying x100..1430 y650..750, with a transparent rectangular inner opening; keep straight track edges and reference's blunt tapered right terminal. Third row: shorter empty stamina rail occupying x100..1150 y900..985, likewise transparent inner opening. Fourth row: one square empty status-effect socket occupying x100..300 y1150..1350, transparent inside. Exactly four independent components, no touching or connecting, no extra art, no drop shadow outside assets. Orthographic front view and all frame rings remain thin enough for large icon artwork. Copy the reference's textures and style very faithfully. IMPORTANT: all bar openings, medallion center, square center, and gaps BETWEEN components are transparent. Keep high-quality material texture, not flat vector outlines. 

## Hollow suffix

HOLLOW skin: cold ash gray eroded iron, fractured circular ring and sparsely chipped rails/square frame. No warm light.

## Human suffix

HUMAN skin: intact worn dark gunmetal ring and rails, tiny restrained burnt-copper amber edges from the reference. No flames.
