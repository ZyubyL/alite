<p align="center">
    <h1>Alite - Asynchronous SQLite library</h1>
</p>

![license](https://img.shields.io/badge/license-Apache%202.0-green?style=flat)
![Tests](https://github.com/zyubyl/alite/actions/workflows/test.yml/badge.svg)
![PyPI version](https://img.shields.io/pypi/v/alite)
![Python versions](https://img.shields.io/pypi/pyversions/alite)

> [!note]
> This project is under development.
> It still has full of bugs.

## Feature checklist

- [x] execute
- [x] executemany
- [ ] fetchone
- [ ] fetchval
- [ ] fetchall
- [ ] row with index and key access

## Installation
```sh
# using uv
uv add alite

# or using pip
python -m pip install alite
```

## Quickstart
```py
import alite

async def main():
    # Accept Path object, raw string path or :memory:
    async with alite.create_pool(":memory:") as pool:
        await pool.execute(
            """
            CREATE TABLE IF NOT EXISTS users
            (name TEXT PRIMARY KEY, age INT NOT NULL)
            """
        )
        cursor = await pool.execute(
            """
            INSERT OR REPLACE INTO users (name, age)
            VALUES (?, ?)
            """,
            ("Alex", 42),
        )
        print(cursor.rowcount) # 1
        cursor = await pool.executemany(
            """
            INSERT OR REPLACE INTO users (name, age)
            VALUES (?, ?)
            """,
            [("Alice", 67), ("Alan", 69)],
        )
        print(cursor.rowcount) # 2
    # pool auto closes when out of the context manager

if __name__ == "__main__":
    import asyncio

    asyncio.run(main())
```

## License

See [LICENSE](LICENSE)
