# Research attribution

The BaseA byte signature and player health/stamina pointer offsets are derived from the
DS2S-META research project by Nordgaren, pseudostripy and contributors, with its
acknowledged reverse-engineering contributors including Lord Radai and TKGP.

Source: https://github.com/pseudostripy/DS2S-META
Revision: b450994925b528addc36706430f5329fe6a2eb41
File: DS2S META/Utils/Offsets/DS2REData.cs

The MIT license supplied by that repository is reproduced in `licenses/DS2S-META.txt`.
No DS2S-META binaries, PropertyHook code or game binaries are bundled. The DS2FX
launcher, native ABI, manifest parser, reader, example and tests are implemented here.

Windows process/debug APIs were checked against Microsoft documentation:
https://learn.microsoft.com/en-us/windows/win32/debug/writing-the-debugger-s-main-loop
https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createremotethread

The separate managed content toolkit has its own SoulsFormats notices and dependencies.

## MinHook 1.3.4

https://github.com/TsudaKageyu/minhook
Revision c3fcafdc10146beb5919319d0683e44e3c30d537 (v1.3.4).
Unmodified source under vendor/minhook; statically linked into Core. BSD-style
licenses for MinHook and HDE are preserved in licenses/MinHook.txt and the vendor
LICENSE.txt. No external DLL is required.

Stamina function semantics at RVA 0x3335d0 were inspected directly in the owned
supported executable. No game executable or extracted code listing is distributed.
The runtime includes its SHA256 and RVA only. See docs/STAMINA-RESEARCH.md.
