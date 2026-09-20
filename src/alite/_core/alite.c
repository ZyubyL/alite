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
    PoolType = (PyTypeObject *)PyType_FromSpec(&Pool_spec);
    ConnectionType = (PyTypeObject *)PyType_FromSpec(&Connection_spec);
    CursorType = (PyTypeObject *)PyType_FromSpec(&Cursor_spec);
    if (!PoolType || !ConnectionType || !CursorType) { return -1; }
    return 0;
}

/*
 * Add created types to the module.
 * Return 0 on OK, -1 with error when fail.
 */
static int add_types(PyObject *mod)
{
    if (PyModule_AddObjectRef(mod, "Pool", (PyObject *)PoolType) < 0) { return -1; }
    if (PyModule_AddObjectRef(mod, "Connection", (PyObject *)ConnectionType) < 0) { return -1; }
    if (PyModule_AddObjectRef(mod, "Cursor", (PyObject *)CursorType) < 0) { return -1; }
    return 0;
}

/*
 * Cleanup created types.
 */
static void cleanup(void)
{
    Py_XDECREF(PoolType);
    Py_XDECREF(ConnectionType);
    Py_XDECREF(CursorType);
    PoolType = NULL;
    ConnectionType = NULL;
    CursorType = NULL;
}

/*
 * Export ALITE_MAX_POOL_SIZE and ALITE_DEFAULT_POOL_SIZE.
 * Return 0 on OK, -1 when fail.
 */
static int export_consts(PyObject *mod)
{
    if (PyModule_AddIntConstant(mod, "MAX_POOL_SIZE", ALITE_MAX_POOL_SIZE) < 0) { return -1; }
    if (PyModule_AddIntConstant(mod, "DEFAULT_POOL_SIZE", ALITE_DEFAULT_POOL_SIZE) < 0) { return -1; }
    return 0;
}

PyMODINIT_FUNC PyInit__alite(void)
{
    PyObject *mod = PyModule_Create(&alite_module);
    if (!mod) { return NULL; }

    if (
        create_types() < 0 ||
        add_types(mod) < 0 ||
        export_consts(mod) < 0
    ) {
        cleanup();
        Py_DECREF(mod);
        return NULL;
    }

    return mod;
}
