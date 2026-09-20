from __future__ import annotations

import pytest

from alite import create_pool


@pytest.mark.parametrize(
    ("pool_size", "should_explode"),
    [
        (1, False),
        (4, False),
        (256, False),
        (0, True),
        (-69, True),
        (420, True),
        (0xFFFF, True),
    ],
)
async def test_pool_size_explosion(pool_size, should_explode):
    if should_explode:
        with pytest.raises((ValueError, RuntimeError)):
            async with create_pool(":memory:", pool_size=pool_size):
                pass
    else:
        async with create_pool(":memory:", pool_size=pool_size) as pool:
            assert pool.pool_size == pool_size


@pytest.mark.parametrize("plastic_bag_count", [0, 1, 69, 1000])
async def test_executemany_mass_production(plastic_bag_count):
    async with create_pool(":memory:") as pool:
        await pool.execute("CREATE TABLE IF NOT EXISTS plastic_bags (name TEXT)")
        rows = [(f"plastic bag number {i}",) for i in range(plastic_bag_count)]
        cur = await pool.executemany("INSERT INTO plastic_bags VALUES (?)", rows)
        assert cur.rowcount == plastic_bag_count
