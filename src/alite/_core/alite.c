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
#include "connection.h"
#include "cursor.h"
#include "pool.h"
#include "row.h"

#define CREATE_MODULE(mod) object *mod = PyModule_Create(&alite_module)

static PyModuleDef alite_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = MODULE_NAME,
    .m_doc  = "Async SQLite3 library - C extension",
    .m_size = -1,
};

/*
 * Create heap types from specs.
 * Return 0 if OK, -1 with error when fail.
 */
static int create_types(void)
{
    PoolType       = (type *)PyType_FromSpec(&Pool_spec);
    ConnectionType = (type *)PyType_FromSpec(&Connection_spec);
    CursorType     = (type *)PyType_FromSpec(&Cursor_spec);
    RowType        = (type *)PyType_FromSpec(&Row_spec);
    if (not PoolType or not ConnectionType or not CursorType or not RowType) { return -1; }
    return 0;
}

/*
 * Add created types to the module.
 * Return 0 on OK, -1 with error when fail.
 */
static int add_types(object *mod)
{
    object *modules[] = {
        (object *)PoolType,
        (object *)ConnectionType,
        (object *)CursorType,
        (object *)RowType,
    };
    const char *module_names[] = {
        "Pool",
        "Connection",
        "Cursor",
        "Row",
    };
    for (usize i = 0; i < LEN(modules); i++) {
        if (PyModule_AddObjectRef(mod, module_names[i], modules[i]) < 0) { return -1; }
    }
    return 0;
}

/*
 * Cleanup created types.
 */
static void cleanup(void)
{
    const type *modules[] = {
        PoolType,
        ConnectionType,
        CursorType,
        RowType,
    };
    for (usize i = 0; i < LEN(modules); i++) {
        Py_CLEAR(modules[i]);
    }
}

/*
 * Export ALITE_MAX_POOL_SIZE and ALITE_DEFAULT_POOL_SIZE.
 * Return 0 on OK, -1 when fail.
 */
static int export_consts(object *mod)
{
    if (PyModule_AddIntConstant(mod, "MAX_POOL_SIZE",     ALITE_MAX_POOL_SIZE    ) < 0) { return -1; }
    if (PyModule_AddIntConstant(mod, "DEFAULT_POOL_SIZE", ALITE_DEFAULT_POOL_SIZE) < 0) { return -1; }
    return 0;
}

PyMODINIT_FUNC PyInit__alite(void)
{
    CREATE_MODULE(mod);
    if (not mod) { return NULL; }

    if (
        create_types() < 0 or
        add_types(mod) < 0 or
        export_consts(mod) < 0
    ) {
        cleanup();
        Py_DECREF(mod);
        return NULL;
    }

    return mod;
}
