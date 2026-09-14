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

from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor

from alite._alite import Cursor as _Cursor
from alite._threading import _run_in_thread

__all__ = ["AsyncCursor"]


class AsyncCursor:
    """
    Async cursor for fetching query results.
    """

    def __init__(self, _cursor: _Cursor, executor: ThreadPoolExecutor) -> None:
        self._cursor = _cursor
        self._executor = executor

        self._closed = False

    def __del__(self) -> None:
        if not self._closed:
            import warnings

            warnings.warn(
                "Unclosed AsyncCursor. Call 'await cursor.close()'",
                ResourceWarning,
                stacklevel=1,
            )

    async def close(self) -> None:
        """
        Close the cursor and release resources.
        """
        if self._closed:
            return
        self._closed = True
        await _run_in_thread(self._executor, self._cursor.close)

    @property
    def rowcount(self) -> int:
        """
        Number of rows affected by the last statement.
        """
        return self._cursor.rowcount
