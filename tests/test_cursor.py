from __future__ import annotations

import pytest

from alite import create_pool


@pytest.mark.parametrize("n", [0, 1, 5, 100])
async def test_fetchall_roundtrip(n):
    async with create_pool(":memory:") as pool:
        await pool.execute("CREATE TABLE t (x INT)")
        rows = [(i,) for i in range(n)]
        await pool.executemany("INSERT INTO t VALUES (?)", rows)
        cur = await pool.execute("SELECT x FROM t ORDER BY x")
        fetched = await cur.fetchall()
        assert [r[0] for r in fetched] == list(range(n))
        # second fetchall returns empty
        assert await cur.fetchall() == []


async def test_fetchone_and_fetchval():
    async with create_pool(":memory:") as pool:
        await pool.execute("CREATE TABLE t (name TEXT, val INT)")
        await pool.execute("INSERT INTO t VALUES (?, ?)", ("a", 1))
        await pool.execute("INSERT INTO t VALUES (?, ?)", ("b", 2))
        cur = await pool.execute("SELECT name, val FROM t ORDER BY val")
        r = await cur.fetchone()
        assert r is not None and r["name"] == "a" and r[1] == 1
        v = await cur.fetchval()
        assert v == "b"
        assert await cur.fetchone() is None
        assert await cur.fetchval() is None


async def test_fetchall_empty_result():
    async with create_pool(":memory:") as pool:
        await pool.execute("CREATE TABLE t (x INT)")
        cur = await pool.execute("SELECT x FROM t")
        assert await cur.fetchall() == []


async def test_rowcount():
    async with create_pool(":memory:") as pool:
        await pool.execute("CREATE TABLE t (x INT)")
        cur = await pool.execute("INSERT INTO t VALUES (?)", (1,))
        assert cur.rowcount == 1
        cur2 = await pool.executemany("INSERT INTO t VALUES (?)", [(2,), (3,)])
        assert cur2.rowcount == 2


async def test_cursor_close():
    async with create_pool(":memory:") as pool:
        await pool.execute("CREATE TABLE t (x INT)")
        cur = await pool.execute("SELECT x FROM t")
        await cur.close()
        with pytest.raises(RuntimeError):
            await cur.fetchall()


async def test_executemany_empty():
    async with create_pool(":memory:") as pool:
        await pool.execute("CREATE TABLE t (x INT)")
        cur = await pool.executemany("INSERT INTO t VALUES (?)", [])
        assert cur.rowcount == 0


async def test_named_params():
    async with create_pool(":memory:") as pool:
        await pool.execute("CREATE TABLE t (name TEXT, age INT)")
        await pool.execute(
            "INSERT INTO t VALUES (:name, :age)", {"name": "alex", "age": 42}
        )
        cur = await pool.execute("SELECT name, age FROM t")
        rows = await cur.fetchall()
        assert rows[0]["name"] == "alex"
        assert rows[0]["age"] == 42
