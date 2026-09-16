# Alite

Async SQLite3 library

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
