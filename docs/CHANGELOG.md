# Alite changelog

Alite uses [Semantic versioning](https://semver.org)

## 0.3.0.alpha0 - [Unreleased]

### Added:

- `Pool.pool_size` property.
- `Cursor.fetchall()`
- `Row` object with index (`Row[0]`) and key access (`Row["name"]`)

### Changed:

- Limit pool size to max 256.

---

## 0.2.0.alpha0 - 2026/09/16

### Added:

- `Pool.execute`, `Pool.executemany`, `Pool.close()`.
- `Cursor.rowcount`, `Cursor.close()`.

---

## 0.1.0.alpha5 - 2026/09/13

### Added:

- `Pool`, `Connection`, `Cursor` (C).
- `AsyncPool` (Python).
