/* Copyright 2026-present ZyubyL
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
**/
#include "alite.h"

static PyModuleDef alite_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = MODULE_NAME,
    .m_doc  = "Async SQLite3 library - C extension",
    .m_size = -1,
};

#define ALITE_INIT_MODULE(mod) PyObject *mod = PyModule_Create(&alite_module)

PyMODINIT_FUNC PyInit__alite(void)
{
    ALITE_INIT_MODULE(mod);
    if (!mod) return NULL;
    return mod;
}
