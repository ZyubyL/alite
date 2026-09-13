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
Generate .clangd for development

Expected to be run at the project root. (src/alite/ is available)
"""

import sys
import sysconfig
from pathlib import Path

cwd = Path.cwd()

no_src = True

for d in cwd.iterdir():
    if d.name == "src":
        for c in d.iterdir():
            if c.name == "alite":
                no_src = False
                break
        break

if no_src:
    print("Couldn't find src/alite/ in the current working directory")
    sys.exit(1)

out = []
out.append("CompileFlags:")
out.append("  Add: [")
out.append("    -x,")
out.append("    c,")  # Explicitly set the header to be C, not C++
out.append("    -std=c2x,")  # This project uses C2X standard
out.append(f"    -I{sysconfig.get_path('include')}")  # For Python.h
out.append("  ]")

clangd = cwd / ".clangd"

clangd.write_text("\n".join(out))
