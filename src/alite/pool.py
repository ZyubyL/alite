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
from typing import Self

from alite._alite import Pool as _Pool
from alite._threading import _run_in_thread

__all__ = ["AsyncPool"]


class AsyncPool:
    """
    Async friendly SQLite connection pool.
    """

    def __init__(self, _pool: _Pool, pool_size: int = 4) -> None:
        self._pool = _pool
        self._executor = ThreadPoolExecutor(
            max_workers=pool_size, thread_name_prefix="alite"
        )
        self._closed = False

    def __del__(self) -> None:
        if not self._closed:
            import warnings

            warnings.warn(
                "Unclosed AsyncPool. Use 'async with' or call 'await pool.close()'",
                ResourceWarning,
                stacklevel=1,
            )

    async def close(self) -> None:
        """
        Close all connections in the pool.
        """
        if self._closed:
            return
        self._closed = True
        # TODO: pool.c Pool_close
        # await _run_in_thread(self._executor, self._pool.close)
        self._executor.shutdown(wait=False)

    async def __aenter__(self) -> Self:
        return self

    async def __aexit__(self, *exc: object) -> None:
        await self.close()
