# Anthology LAN DX11

This worktree is the LAN branch based on Anthology v170 (`12288d0`). Its DirectPlay
transport hosts the restored arena multiplayer modes. The Steam implementation
from the supplied reference archive is not enabled in this build. Campaign
co-op, ALife replication and wholesale compatibility with SP addons are future
work, not features of this release.

Build both variants from this checkout:

```powershell
py -3 tools/build_lan.py
```

Requires VS2022 C++ Build Tools, Windows SDK, the repository SDK/submodules and
the same external upscaler SDK configuration as the SP engine. Run
`git submodule update --init` in a new checkout. Outputs default to
`../dist/bin/AnomalyDX11_LAN.exe` and `AnomalyDX11AVX_LAN.exe`. Build logs and hashes
go to `../build-logs`. No command here installs into a running game.

The workspace's shared entry point is
`projects/anthology-runtime-seasons-integration/build_dx11_family.py`. It builds
SP DX11, SP DX11 AVX, LAN DX11 and LAN DX11 AVX from their own source trees.
For subsequent releases merge reviewed SP changes into the LAN branch, preserve
the network adaptations, and run that matrix. Do not replace a LAN binary with
an SP binary just because both report the same date.

Payloads and provenance are maintained in the enclosing `anthology-lan-engine`
project. `prepare_payload.py` adapts the supplied network database;
`prepare_renderer.py` freezes the active shader providers without SP Lua mods.
Both LAN peers must use the same release, network database and base modpack.

Ported native changes cover disconnect event locking, safe entity teardown,
remote actor ownership, local weapon/HUD access, spawn camera history, the
first-bullet cone, server-authoritative mines and mine kill credit, one-shot
death-bag claims and exact-identity shop helper removal. Proximity mines inspect
at most 32 session players per scheduled update, rather than all world objects.
