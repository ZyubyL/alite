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

import asyncio
from collections.abc import Callable
from concurrent.futures import ThreadPoolExecutor
from typing import Any


async def _run_in_thread(
    executor: ThreadPoolExecutor, fn: Callable[..., Any], *args: Any
) -> Any:
    """
    Run a blocking function in the thread pool executor.
    """
    loop = asyncio.get_running_loop()
    return await loop.run_in_executor(executor, fn, *args)
