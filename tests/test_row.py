from __future__ import annotations

import asyncio

import pytest

from alite import create_pool


async def _rows():
    async with create_pool(":memory:") as pool:
        await pool.execute("CREATE TABLE t (name TEXT, age INT)")
        await pool.execute("INSERT INTO t VALUES (?, ?)", ("alex", 42))
        await pool.execute("INSERT INTO t VALUES (?, ?)", ("bob", 67))
        cur = await pool.execute("SELECT name, age FROM t ORDER BY age")
        return await cur.fetchall()


async def test_row_index_and_key():
    rows = await _rows()
    r = rows[0]
    assert r[0] == "alex"
    assert r["age"] == 42
    assert r[-1] == 42
    with pytest.raises(IndexError):
        _ = r[5]
    with pytest.raises(IndexError):
        _ = r["nope"]


async def test_row_len_iter_keys():
    rows = await _rows()
    r = rows[0]
    assert len(r) == 2
    assert list(r) == ["alex", 42]
    assert r.keys() == ["name", "age"]


async def test_row_repr():
    rows = await _rows()
    assert "alite.Row" in repr(rows[0])


async def test_row_equality():
    rows = await _rows()
    assert rows[0] != ("bob", 67)
    assert rows[0] == ("alex", 42)
    assert rows[0] == rows[0]


async def test_row_iter_in_comprehension():
    rows = await _rows()
    names = [r["name"] for r in rows]
    assert names == ["alex", "bob"]
