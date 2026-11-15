# Changelog

All notable changes to this project are documented here.
本文件记录项目的重要变更。

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versioning follows [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- Keil MDK project `Project/G407_EnvNode.uvprojx` covering all source groups, verified to build with 0 errors / 0 warnings.
- CMSIS and STM32F4xx standard peripheral driver under `Libraries/` so the repository builds without extra downloads.
- Bilingual `README.md` with architecture diagram, register summary and quick start.
- `docs/modbus-protocol.md`: register map, frame layout, exception codes and CRC-verified example frames.
- `docs/hardware.md`: pin assignment, alternate functions, wiring notes and power-on behaviour.
- `docs/getting-started.md`: toolchain, build, flash, host bring-up and troubleshooting table.
- `docs/architecture.md`: layering, ISR/main-loop cooperation, timing and design trade-offs.
- `docs/known-issues.md`: known bugs, spec gaps, engineering hygiene and roadmap.
- `tools/modbus_poll.py`: host-side polling utility with `--selftest` (CRC and frame decoding) and `--raw` mode.
- `.gitignore` rules for Keil build artifacts and Python; `.gitattributes` binary markers.

### Changed

- Repository reorganised from a flat file list into `App/`, `BSP/`, `Driver/`, `Display/`, `User/`, `Libraries/`, `Project/`, `docs/`, `tools/`.
- `README.md` rewritten (was a two-line placeholder).
- Keil target renamed `TEST` → `G407_EnvNode`; output artefacts renamed accordingly.

### Removed

- Empty stray file `BaseDrive` (accidental artefact, not a directory).
- Unused HMAC sources and ETH driver that were not part of this application.

### Notes

- Firmware behaviour is unchanged: no C source was modified in this round. Known code issues are documented, not fixed, in `docs/known-issues.md`.

## [0.1.0] - 2026-11-01

- Initial upload: application sources for the STM32F407 environmental node (Modbus RTU slave, PMS5003, BH1750, TFT display).
