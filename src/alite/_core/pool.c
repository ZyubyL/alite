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
#include "pool.h"
#include "alite.h"
#include "connection.h"

static i8 Pool_init(PoolObject *self, PyObject *args, PyObject *kwargs)
{
    const char *path;
    i64 pool_size = ALITE_DEFAULT_POOL_SIZE;

    static char *kwlist[] = { "path", "pool_size", nullptr };
    if (
        !PyArg_ParseTupleAndKeywords(args, kwargs, "s|i", kwlist, &path, &pool_size)
    ) { return -1; }
    self->path = strdup(path);
    if (!self->path) { goto nomem; }

    if (pool_size <= 0) {
        PyErr_SetString(PyExc_ValueError, "pool_size must be greater than 0");
        goto cleanup;
    }
    self->pool_size = pool_size;
    self->connections = calloc(pool_size, sizeof(ConnectionObject*));
    if (!self->connections) { goto nomem; }

    return 0;

nomem:
    PyErr_NoMemory();
cleanup:
    free(self->connections);
    self->connections = nullptr;
    free(self->path);
    self->path = nullptr;
    return -1;
}

static void Pool_dealloc(PoolObject *self)
{
    free(self->connections);
    free(self->path);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject* Pool_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    PoolObject *self = (PoolObject *)type->tp_alloc(type, 0);
    if (self) {
        self->connections = nullptr;
        self->pool_size = 0;
        self->path = nullptr;
    }
    return (PyObject *)self;
}

PyTypeObject PoolType = {
    PyVarObject_HEAD_INIT(nullptr, 0)
    .tp_name      = MODULE_NAME ".Pool",
    .tp_basicsize = sizeof(PoolObject),
    .tp_doc       = "SQLite connection pool",
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_init      = (initproc)Pool_init,
    .tp_dealloc   = (destructor)Pool_dealloc,
    .tp_new       = Pool_new,
};
