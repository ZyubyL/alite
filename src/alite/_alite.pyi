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
Type stubs for alite._alite C extension.
"""

from __future__ import annotations

from collections.abc import Sequence
from typing import Any

class Pool:
    """
    SQLite connection pool.
    """
    def __init__(self, path: str, pool_size: int = 4) -> None: ...
    def close(self) -> None: ...
    def execute(
        self, sql: str, params: tuple[Any, ...] | Sequence[Any] = ()
    ) -> Cursor: ...
    def executemany(
        self, sql: str, params_list: Sequence[tuple[Any, ...]]
    ) -> Cursor: ...
    @property
    def pool_size(self) -> int: ...

class Connection:
    """
    SQLite database connection.
    """
    def close(self) -> None: ...

class Cursor:
    """
    SQLite query cursor.
    """
    @property
    def rowcount(self) -> int: ...
    def close(self) -> None: ...
