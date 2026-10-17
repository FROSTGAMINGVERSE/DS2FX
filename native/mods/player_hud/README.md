# Player HUD 0.2.0

Requires DS2FX Core 0.3.0-alpha.1 / API 2. Observer is optional. Shows numeric HP,
a health bar, low-health warning and optional numeric stamina with a second bar.
Windows layered click-through overlay; windowed/borderless only, not exclusive
fullscreen. It follows the game client, hides on focus loss, absent character or
stale snapshots, and does not draw into the game renderer. Game-only captures may
omit it. No health, stamina or other gameplay values are changed by this module.

Edit the installed mod's `Root/Game/DS2FX/Plugins/frost-player-hud/hud.ini` with the
game stopped and restart. Settings in [HUD]:

| Setting | Default | Meaning |
|---|---:|---|
| Enabled | 1 | 0 disables the overlay |
| ShowStamina | 1 | 0 restores the compact health-only panel |
| LowHealthPercent | 30 | Inclusive warning threshold, 1..100 |
| AnchorRight | 1 | 1 top-right; 0 top-left |
| MarginX / MarginY | 24 / 24 | Logical pixel insets, 0..4096 |
| ScalePercent | 100 | 50..200, plus window DPI scaling |
| Opacity | 240 | Whole-panel alpha, 32..255 |

Invalid readings show Unavailable for stamina while retaining valid HP. Negative
stamina is displayed as zero for a readable bar; the API retains the raw value.
Invalid settings fall back to defaults. Configuration is never written by the DLL.

HUD 0.1.1 was visibly verified at 727/826, LOW HEALTH 119/826 and healed 499/826,
with clean Root Builder restoration. Version 0.2 passes ABI, logic, rendered color,
window ownership/click-through and bootstrap tests; its stamina row was subsequently confirmed in live session37040, including
expenditure and full recovery. No multiplayer or extended stability certification exists.

Rollback: exit, let Root Builder Clear, disable this optional mod. Restore the
previous Core/Observer/HUD set together when downgrading API versions.
