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
#include "cursor.h"

/*
 * Find a free connection in the pool.
 * Return the connection on success, NULL on failure.
 */
static inline ConnectionObject* find_free_connection(PoolObject *self)
{
    for (i64 i = 0; i < self->opened_conns; i++) {
        if (!self->connections[i]->in_use && self->connections[i]->db) {
            self->connections[i]->in_use = 1;
            return self->connections[i];
        }
    }
    return NULL;
}

/*
 * Create a new connection if under pool size.
 * Return the connection on success, NULL on failure.
 */
static inline ConnectionObject* new_connection(PoolObject *self)
{
    if (self->opened_conns < self->pool_size) {
        ConnectionObject *conn = PyObject_New(ConnectionObject, &ConnectionType);
        if (!conn) { return NULL; }
        conn->db = NULL;
        conn->in_use = 1;
        if (Connection_open_db(conn, self->path) != 0) {
            Py_DECREF(conn);
            return NULL;
        }

        self->connections[self->opened_conns++] = conn;
        return conn;
    }
    return NULL;
}

ConnectionObject* Pool_get_connection(PoolObject *self)
{
    PyThread_acquire_lock(self->lock, WAIT_LOCK);
    if (self->closed) {
        PyThread_release_lock(self->lock);
        PyErr_SetString(PyExc_RuntimeError, "Pool is closed");
        return NULL;
    }

    ConnectionObject *conn = find_free_connection(self);
    if (conn) { goto success; }

    conn = new_connection(self);
    if (conn) { goto success; }

    // Failure
    PyThread_release_lock(self->lock);
    PyErr_SetString(PyExc_RuntimeError, "Cannot get connection");
    return NULL;

success:
    PyThread_release_lock(self->lock);
    return conn;
}

void Pool_return_connection(PoolObject *self, ConnectionObject *conn)
{
    PyThread_acquire_lock(self->lock, WAIT_LOCK);
    conn->in_use = 0;
    PyThread_release_lock(self->lock);
}

#define NEW_CURSOR(cur) \
    CursorObject *cur = PyObject_New(CursorObject, &CursorType); \
    if (cur) { \
        (cur)->stmt = NULL; \
        (cur)->conn = NULL; \
        (cur)->closed = 0; \
    }

/* Pool.execute(sql, params) */
static PyObject* Pool_execute(PoolObject *self, PyObject *args, PyObject *kwargs)
{
    const char *sql;
    PyObject *params = NULL;

    static char *kwlist[] = { "sql", "params", NULL };
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|O", kwlist, &sql, &params)) { return NULL; }

    ConnectionObject *conn = Pool_get_connection(self);
    if (!conn) { return NULL; }

    NEW_CURSOR(cur);
    if (!cur) { goto failure; }
    Py_INCREF(Py_None);

    if (Cursor_prepare(cur, conn, sql, params) != 0) {
        Py_DECREF(cur);
        goto failure;
    }

    Pool_return_connection(self, conn);
    return (PyObject *)cur;

failure:
    Pool_return_connection(self, conn);
    return NULL;
}

static i8 Pool_init(PoolObject *self, PyObject *args, PyObject *kwargs)
{
    const char *path;
    i64 pool_size = ALITE_DEFAULT_POOL_SIZE;

    static char *kwlist[] = { "path", "pool_size", NULL };
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
    self->opened_conns = 0;
    self->connections = calloc(pool_size, sizeof(ConnectionObject*));
    if (!self->connections) { goto nomem; }

    self->lock = PyThread_allocate_lock();
    if (!self->lock) { goto nomem; }

    self->lock_init = 1;
    return 0;

nomem:
    PyErr_NoMemory();
cleanup:
    free(self->connections);
    self->connections = NULL;
    free(self->path);
    self->path = NULL;
    if (self->lock) {
        PyThread_free_lock(self->lock);
        self->lock = NULL;
    }
    return -1;
}

static void Pool_dealloc(PoolObject *self)
{
    for (i64 i = 0; i < self->opened_conns; i++) {
        Connection_close_db(self->connections[i]);
        Py_DECREF(self->connections[i]);
    }
    free(self->connections);
    free(self->path);
    if (self->lock_init && self->lock_init) { PyThread_free_lock(self->lock); }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject* Pool_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    PoolObject *self = (PoolObject *)type->tp_alloc(type, 0);
    if (self) {
        self->connections = NULL;
        self->opened_conns = 0;
        self->pool_size = 0;
        self->path = NULL;
        self->lock = NULL;
        self->lock_init = 0;
        self->closed = 0;
    }
    return (PyObject *)self;
}

static PyObject* Pool_close(PoolObject *self, PyObject *args, PyObject *kwargs)
{
    PyThread_acquire_lock(self->lock, WAIT_LOCK);
    self->closed = 1;
    for (i64 i = 0; i < self->opened_conns; i++) {
        ConnectionObject *c = self->connections[i];
        Connection_close_db(c);
        c->in_use = 0;
    }
    PyThread_release_lock(self->lock);
    Py_RETURN_NONE;
}

static PyMethodDef Pool_methods[] = {
    { "execute", (PyCFunction)Pool_execute, METH_VARARGS | METH_KEYWORDS, "Execute SQL, return cursor" },
    { "close", (PyCFunction)Pool_close, METH_NOARGS, "Close all connections in the pool" },
    { NULL },
};

PyTypeObject PoolType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = MODULE_NAME ".Pool",
    .tp_basicsize = sizeof(PoolObject),
    .tp_doc       = "SQLite connection pool",
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_init      = (initproc)Pool_init,
    .tp_dealloc   = (destructor)Pool_dealloc,
    .tp_new       = Pool_new,
    .tp_methods = Pool_methods,
};
