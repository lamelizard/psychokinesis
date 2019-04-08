# Changelog

## Migration Guide

### to 0.9

* `glm` is an external dependency now (applications need to provide it themselves and make a target `glm` available)
* `ColorSpace` is now mandatory when loading textures

## Changes

### 0.9

* `glow::info()` now works with `glm` types again (and anything that either has `operator<<` or `to_string`)
* removed `fmt` and `snappy` dependencies
* made `glm` an externally-provided dependency
* CMake is now multi-config friendly (a single `.sln` for VS is sufficient now)
