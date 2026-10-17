# Schema provenance

These four public DS2 Scholar field definitions are from Smithbox:
https://github.com/vawser/Smithbox/tree/5a25111029853b7bf2b43cb41b752f67baa66344/src/Smithbox.Data/Assets/PARAM/DS2S/Defs

Retrieved 2026-09-25. The game payload is little endian; these definitions describe
field layouts only. The tool preserves the input PARAM's format and endianness.
No game PARAM data is bundled here. Existing original row byte ranges are compared
after serializing, and all untouched binder entries remain byte identical.

SpEffect argument meanings are from LordRadai/FRPG2Emedf commit
c55446c361089dc00b16bfc20d38e082ca52c96f. Exact instruction bytes are checked against
the user's own regulation before generating a new oil effect.
