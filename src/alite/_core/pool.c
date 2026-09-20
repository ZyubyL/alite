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
#include "bind.h"
#include "connection.h"
#include "cursor.h"

/*
 * Find a free connection in the pool.
 * Return the connection on success, NULL on failure.
 */
static inline ConnectionObject* find_free_connection(PoolObject *self)
{
    for (usize i = 0; i < self->opened_conns; i++) {
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

/*
 * Execute an SQL statement (no results). Release GIL when executing.
 * Return SQLITE_OK on success.
 */
static i8 exec(sqlite3 *db, const char *sql)
{
    char *errmsg = NULL;
    i8 rc;
    Py_BEGIN_ALLOW_THREADS
    rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    Py_END_ALLOW_THREADS
    if (errmsg) { sqlite3_free(errmsg); }
    return rc;
}

/*
 * Prepare an SQL statement. Release GIL when doing.
 * Return SQLITE_OK on success.
 */
static i8 prepare(sqlite3* db, const char *sql, sqlite3_stmt **out)
{
    i8 rc;
    Py_BEGIN_ALLOW_THREADS
    rc = sqlite3_prepare_v2(db, sql, -1, out, NULL);
    Py_END_ALLOW_THREADS
    return rc;
}

/*
 * Rollback transaction on error, finalize statement, release resources, set the error and return NULL;
 */
static void rollback(PoolObject *self, ConnectionObject *conn, CursorObject *cur, PyObject *seq, i8 managed, const char *msg)
{
    if (managed) { exec(conn->db, "ROLLBACK"); }

    if (cur && cur->stmt) {
        sqlite3_finalize(cur->stmt);
        cur->stmt = NULL;
    }

    Py_XDECREF(seq);
    Py_XDECREF(cur);
    Pool_return_connection(self, conn);
    PyErr_Format(PyExc_RuntimeError, "%s: %s", msg, sqlite3_errmsg(conn->db));
}

/*
 * Execute prepared statement for each row in the seq.
 * Return the number of rows executed, or -1 on error.
 */
static Py_ssize_t executemany_loop(sqlite3 *db, sqlite3_stmt *stmt, PyObject *seq, Py_ssize_t count, int managed_txn)
{
    for (Py_ssize_t i = 0; i < count; i++) {
        PyObject *row = PySequence_Fast_GET_ITEM(seq, i);
        if (row && row != Py_None) {
            if (alite_bind_positional(stmt, row) != SQLITE_OK) {
                PyErr_Format(PyExc_RuntimeError, "Bind failed: %s", sqlite3_errmsg(db));
                goto failure;
            }
        }

        i8 rc;
        Py_BEGIN_ALLOW_THREADS
        rc = sqlite3_step(stmt);
        Py_END_ALLOW_THREADS
        if (rc != SQLITE_DONE) {
            PyErr_Format(PyExc_RuntimeError, "Step failed: %s", sqlite3_errmsg(db));
            goto failure;
        }
        sqlite3_reset(stmt);
    }
    return count;

failure:
    if (managed_txn) { exec(db, "ROLLBACK"); }
    return -1;
}

static PyObject* Pool_get_pool_size(PoolObject *self, void *closure)
{
    return PyLong_FromUnsignedLong(self->pool_size);
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

/* Pool.executemany(sql, list[params]) */
static PyObject* Pool_executemany(PoolObject *self, PyObject *args, PyObject *kwargs)
{
    const char *sql;
    PyObject *params_list;

    static char *kwlist[] = { "sql", "params_list", NULL };
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sO", kwlist, &sql, &params_list)) { return NULL; }

    ConnectionObject *conn = Pool_get_connection(self);
    if (!conn) { return NULL; }

    NEW_CURSOR(cur);
    if (!cur) { goto failure; }
    Py_INCREF(Py_None);

    PyObject *seq = PySequence_Fast(params_list, "params_list must be a sequence");
    if (!seq) {
        Py_DECREF(cur);
        goto failure;
    }

    Py_ssize_t count = PySequence_Fast_GET_SIZE(seq);
    if (count == 0) {
        Py_DECREF(seq);
        Pool_return_connection(self, conn);
        return (PyObject *)cur;
    }

    i8 managed_txn = sqlite3_get_autocommit(conn->db);
    if (
        managed_txn
        && exec(conn->db, "BEGIN IMMEDIATE") != SQLITE_OK
    ) {
        Py_DECREF(seq);
        Py_DECREF(cur);
        PyErr_Format(PyExc_RuntimeError, "BEGIN failed: %s", sqlite3_errmsg(conn->db));
        goto failure;
    }

    if(prepare(conn->db, sql, &cur->stmt) != SQLITE_OK) {
        rollback(self, conn, cur, seq, managed_txn, "Prepare failed");
    }

    Py_ssize_t executed = executemany_loop(conn->db, cur->stmt, seq, count, managed_txn);
    Py_DECREF(seq);
    if (executed < 0) {
        sqlite3_finalize(cur->stmt);
        cur->stmt = NULL;
        Py_DECREF(cur);
        goto failure;
    }

    sqlite3_finalize(cur->stmt);
    cur->stmt = NULL;
    cur->rowcount = (i64)executed;

    if (
        managed_txn
        && exec(conn->db, "COMMIT") != SQLITE_OK
    ) {
        Py_DECREF(cur);
        PyErr_Format(PyExc_RuntimeError, "COMMIT failed: %s", sqlite3_errmsg(conn->db));
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
        !PyArg_ParseTupleAndKeywords(args, kwargs, "s|k", kwlist, &path, &pool_size)
    ) { return -1; }
    self->path = strdup(path);
    if (!self->path) { goto nomem; }

    if (pool_size < 1 || pool_size > ALITE_MAX_POOL_SIZE) {
        PyErr_Format(PyExc_ValueError, "pool_size must be between 1 and %lu", ALITE_MAX_POOL_SIZE);
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
    for (usize i = 0; i < self->opened_conns; i++) {
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
    for (usize i = 0; i < self->opened_conns; i++) {
        ConnectionObject *c = self->connections[i];
        Connection_close_db(c);
        c->in_use = 0;
    }
    PyThread_release_lock(self->lock);
    Py_RETURN_NONE;
}

static PyMethodDef Pool_methods[] = {
    { "execute", (PyCFunction)Pool_execute, METH_VARARGS | METH_KEYWORDS, "Execute SQL, return cursor" },
    { "executemany", (PyCFunction)Pool_executemany, METH_VARARGS | METH_KEYWORDS, "Execute SQL for each param set" },
    { "close", (PyCFunction)Pool_close, METH_NOARGS, "Close all connections in the pool" },
    { NULL },
};

static PyGetSetDef Pool_getset[] = {
    { "pool_size", (getter)Pool_get_pool_size, NULL, "Number of rows affected", NULL },
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
    .tp_methods   = Pool_methods,
    .tp_getset    = Pool_getset,
};
