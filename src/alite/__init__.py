# Copyright 2026-present ZyubyL
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Async SQLite3 library
"""

from __future__ import annotations

from importlib.metadata import version
from pathlib import Path

from alite._alite import DEFAULT_POOL_SIZE
from alite._alite import Pool as _Pool
from alite.pool import AsyncPool

__version__ = version("alite")
"""
See current alite version.
"""


def create_pool(database: str | Path, pool_size: int = DEFAULT_POOL_SIZE) -> AsyncPool:
    """
    Create an async connection pool to an SQLite database.

    Args:
        database:
            Path to the SQLite database file, or ':memory:' to run it on memory.
        pool_size:
            Maximum number of connections in the pool.

    Examples:
        ```py
        async with create_pool("foo/bar.db") as pool:
            ...
        # Pool auto closes with async with context manager.
        ```
    """
    return AsyncPool(_Pool(str(database), pool_size))


__all__ = ["AsyncPool", "create_pool"]
