# Changelog

All notable changes to this project will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Version 1.5.0] - 2022-11-14

### Added

- Added grid-cells.

### Changed

- Cleanup of JSON output.

### Fixed

- Fixed bug in pack mode compact.
- Fixed pack mode compact without trim convex.
- Fixed allow-rotate with pack mode compact.
- Fixed Visual Studio compile flags for catch2.

## [Version 1.4.0] - 2022-10-15

### Added

- Added layers (issue #4).
- Support mixed number and word pivot declarations. (e.g. pivot center 8)
- Allow to offset pivot points (e.g. pivot top + 3 right - 5)

### Changed

- Removed old definition names sheet/texture for input/output.
- Improved invalid argument count error messages.

### Fixed

- Fixed -- commandline arguments.
- Fixed sample cpp.template.

[version 1.5.0]: https://github.com/houmain/spright/compare/1.4.0...1.5.0
[version 1.4.0]: https://github.com/houmain/spright/compare/1.3.1...1.4.0