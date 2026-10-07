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


/* Require self as PoolObject pointer in scope */
#define ACQUIRE_LOCK_GIL                          \
do {                                              \
    Py_BEGIN_ALLOW_THREADS                        \
    PyThread_acquire_lock(self->lock, WAIT_LOCK); \
    Py_END_ALLOW_THREADS                          \
} while(0)

/* Require self as PoolObject pointer in scope */
#define RELEASE_LOCK PyThread_release_lock(self->lock)

/*
 * Find a free connection in the pool.
 * Return the connection on success, NULL on failure.
 */
static inline ConnectionObject* find_free_connection(PoolObject *self)
{
    for (usize i = 0; i < self->opened_conns; i++) {
        if (not self->connections[i]->in_use and self->connections[i]->db) {
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
        ConnectionObject *conn = PyObject_New(ConnectionObject, ConnectionType);
        if (not conn) { return NULL; }
        MAKE_NULL(conn->db);
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
    ACQUIRE_LOCK_GIL;
    if (self->closed) {
        RELEASE_LOCK;
        PyErr_SetString(RuntimeError, "Pool is closed");
        return NULL;
    }

    ConnectionObject *conn = find_free_connection(self);
    if (conn) { goto success; }

    conn = new_connection(self);
    if (conn) { goto success; }

    // Failure
    RELEASE_LOCK;
    PyErr_SetString(RuntimeError, "Cannot get connection");
    return NULL;

success:
    RELEASE_LOCK;
    return conn;
}

void Pool_return_connection(PoolObject *self, ConnectionObject *conn)
{
    ACQUIRE_LOCK_GIL;
    conn->in_use = 0;
    RELEASE_LOCK;
}

static CursorObject* new_cursor()
{
    CursorObject *cur = PyObject_New(CursorObject, CursorType);
    if (cur) {
        MAKE_NULL(cur->stmt);
        MAKE_NULL(cur->conn);
        MAKE_NULL(cur->col_names);
        MAKE_NULL(cur->col_index);
        cur->closed = 0;
        cur->done = 0;
        cur->rowcount = 0;
        return cur;
    }
    return NULL;
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
static void rollback(PoolObject *self, ConnectionObject *conn, CursorObject *cur, object *seq, i8 managed, const char *msg)
{
    if (managed) { exec(conn->db, "ROLLBACK"); }

    if (cur and cur->stmt) {
        sqlite3_finalize(cur->stmt);
        MAKE_NULL(cur->stmt);
    }

    Py_XDECREF(seq);
    Py_XDECREF(cur);
    Pool_return_connection(self, conn);
    PyErr_Format(RuntimeError, "%s: %s", msg, sqlite3_errmsg(conn->db));
}

/*
 * Execute prepared statement for each row in the seq.
 * Return the number of rows executed, or -1 on error.
 */
static isize executemany_loop(sqlite3 *db, sqlite3_stmt *stmt, object *seq, isize count, int managed_txn)
{
    for (isize i = 0; i < count; i++) {
        object *row = PySequence_Fast_GET_ITEM(seq, i);
        if (row and row != None) {
            if (alite_bind_positional(stmt, row) != SQLITE_OK) {
                PyErr_Format(RuntimeError, "Bind failed: %s", sqlite3_errmsg(db));
                goto failure;
            }
        }

        i8 rc = stmt_step_gil(stmt);
        if (rc != SQLITE_DONE) {
            PyErr_Format(RuntimeError, "Step failed: %s", sqlite3_errmsg(db));
            goto failure;
        }
        sqlite3_reset(stmt);
    }
    return count;

failure:
    if (managed_txn) { exec(db, "ROLLBACK"); }
    return -1;
}

/*
 * BEGIN IMMEDIATE when auto commit is on.
 * Return 0 on OK, -1 with error when fail.
 */
static i8 begin_managed_txn(ConnectionObject *conn, i8 *out_managed)
{
    i8 managed = sqlite3_get_autocommit(conn->db);
    *out_managed = managed;
    if (managed and exec(conn->db, "BEGIN IMMEDIATE") != SQLITE_OK) {
        PyErr_Format(RuntimeError, "BEGIN failed: %s", sqlite3_errmsg(conn->db));
        return -1;
    }
    return 0;
}

/*
 * Commit when managed.
 * Return 0 on OK, -1 with error when fail.
 */
static i8 commit_managed_txn(ConnectionObject *conn, i8 managed)
{
    if (managed and exec(conn->db, "COMMIT") != SQLITE_OK) {
        PyErr_Format(RuntimeError, "COMMIT failed: %s", sqlite3_errmsg(conn->db));
        return -1;
    }
    return 0;
}

static i8 prepare_or_rollback(PoolObject *pool, ConnectionObject *conn, CursorObject *cur, object *seq, const char *sql, i8 managed)
{
    if (prepare(conn->db, sql, &cur->stmt) != SQLITE_OK) {
        rollback(pool, conn, cur, seq, managed, "Prepare failed");
        return -1;
    }
    return 0;
}

static object* Pool_get_pool_size(PoolObject *self, void *closure)
{
    return PyLong_FromUnsignedLong(self->pool_size);
}

/* Pool.execute(sql, params) */
static object* Pool_execute(PoolObject *self, object *args, object *kwargs)
{
    const char *sql;
    object *params = NULL;

    static char *kwlist[] = { "sql", "params", NULL };
    if (not PyArg_ParseTupleAndKeywords(args, kwargs, "s|O", kwlist, &sql, &params)) { return NULL; }

    ConnectionObject *conn = Pool_get_connection(self);
    if (not conn) { return NULL; }

    CursorObject *cur = new_cursor();
    if (not cur) { goto failure; }

    if (Cursor_prepare(cur, conn, sql, params) != 0) {
        Py_DECREF(cur);
        goto failure;
    }

    Pool_return_connection(self, conn);
    return (object *)cur;

failure:
    Pool_return_connection(self, conn);
    return NULL;
}

/* Pool.executemany(sql, list[params]) */
static object* Pool_executemany(PoolObject *self, object *args, object *kwargs)
{
    const char *sql;
    object *params_list;

    static char *kwlist[] = { "sql", "params_list", NULL };
    if (not PyArg_ParseTupleAndKeywords(args, kwargs, "sO", kwlist, &sql, &params_list)) { return NULL; }

    ConnectionObject *conn = Pool_get_connection(self);
    if (not conn) { return NULL; }

    CursorObject *cur = new_cursor();
    if (not cur) { goto failure; }

    object *seq = PySequence_Fast(params_list, "params_list must be a sequence");
    if (not seq) {
        Py_DECREF(cur);
        goto failure;
    }

    isize count = PySequence_Fast_GET_SIZE(seq);
    if (count == 0) {
        Py_DECREF(seq);
        Pool_return_connection(self, conn);
        return (object *)cur;
    }

    i8 managed_txn = 0;
    if (begin_managed_txn(conn, &managed_txn) != 0) {
        Py_DECREF(seq);
        Py_DECREF(cur);
        goto failure;
    }
    if (prepare_or_rollback(self, conn, cur, seq, sql, managed_txn) != 0) {
        return NULL;
    }

    isize executed = executemany_loop(conn->db, cur->stmt, seq, count, managed_txn);
    Py_DECREF(seq);
    if (executed < 0) {
        sqlite3_finalize(cur->stmt);
        MAKE_NULL(cur->stmt);
        Py_DECREF(cur);
        goto failure;
    }

    sqlite3_finalize(cur->stmt);
    MAKE_NULL(cur->stmt);
    cur->rowcount = (i64)executed;

    if (commit_managed_txn(conn, managed_txn) != 0) {
        Py_DECREF(cur);
        goto failure;
    }

    Pool_return_connection(self, conn);
    return (object *)cur;

failure:
    Pool_return_connection(self, conn);
    return NULL;
}

static int Pool_init(PoolObject *self, object *args, object *kwargs)
{
    const char *path;
    i64 pool_size = ALITE_DEFAULT_POOL_SIZE;

    static char *kwlist[] = { "path", "pool_size", NULL };
    if (
        not PyArg_ParseTupleAndKeywords(args, kwargs, "s|k", kwlist, &path, &pool_size)
    ) { return -1; }

    if (pool_size < 1 or pool_size > ALITE_MAX_POOL_SIZE) {
        PyErr_Format(ValueError, "pool_size must be between 1 and %lu", ALITE_MAX_POOL_SIZE);
        goto cleanup;
    }

    self->path = strdup(path);
    if (not self->path) {
        PyErr_NoMemory();
        goto cleanup;
    }

    self->pool_size = pool_size;
    self->opened_conns = 0;
    self->connections = calloc(pool_size, sizeof(ConnectionObject*));
    if (not self->connections) {
        PyErr_NoMemory();
        goto cleanup;
    }

    self->lock = PyThread_allocate_lock();
    if (not self->lock) {
        PyErr_NoMemory();
        goto cleanup;
    }

    self->lock_init = 1;
    return 0;

cleanup:
    free(self->connections);
    MAKE_NULL(self->connections);
    free(self->path);
    MAKE_NULL(self->path);
    if (self->lock) {
        PyThread_free_lock(self->lock);
        MAKE_NULL(self->lock);
    }
    self->pool_size = 0;
    self->lock_init = 0;
    self->opened_conns = 0;
    return -1;
}

static void Pool_dealloc(PoolObject *self)
{
    if (self->connections) {
        for (usize i = 0; i < self->opened_conns; i++) {
            Connection_close_db(self->connections[i]);
            Py_DECREF(self->connections[i]);
        }
        free(self->connections);
    }
    if (self->path) { free(self->path); }
    if (self->lock_init and self->lock) { PyThread_free_lock(self->lock); }
    FREE_OBJ;
}

static object* Pool_new(type *type, object *args, object *kwargs)
{
    PoolObject *self = (PoolObject *)type->tp_alloc(type, 0);
    if (self) {
        MAKE_NULL(self->connections);
        self->opened_conns = 0;
        self->pool_size = 0;
        MAKE_NULL(self->path);
        MAKE_NULL(self->lock);
        self->lock_init = 0;
        self->closed = 0;
    }
    return (object *)self;
}

static object* Pool_close(PoolObject *self, object *args, object *kwargs)
{
    ACQUIRE_LOCK_GIL;
    self->closed = 1;
    for (usize i = 0; i < self->opened_conns; i++) {
        ConnectionObject *c = self->connections[i];
        Connection_close_db(c);
        c->in_use = 0;
    }
    RELEASE_LOCK;
    Py_RETURN_NONE;
}

static PyMethodDef Pool_methods[] = {
    { "execute", (PyCFunction)Pool_execute, METH_VARARGS | METH_KEYWORDS, "Execute SQL, return cursor" },
    { "executemany", (PyCFunction)Pool_executemany, METH_VARARGS | METH_KEYWORDS, "Execute SQL for each param set" },
    { "close", (PyCFunction)Pool_close, METH_NOARGS, "Close all connections in the pool" },
    { NULL },
};

static PyGetSetDef Pool_getset[] = {
    { "pool_size", (getter)Pool_get_pool_size, NULL, "Number of pool size initiated", NULL },
    { NULL },
};

static PyType_Slot Pool_slots[] = {
    { Py_tp_doc,     "SQLite connection pool" },
    { Py_tp_init,    Pool_init                },
    { Py_tp_new,     Pool_new                 },
    { Py_tp_dealloc, Pool_dealloc             },
    { Py_tp_getset,  Pool_getset              },
    { Py_tp_methods, Pool_methods             },
    { 0,             NULL                     },
};

PyType_Spec Pool_spec = {
    .name      = MODULE_NAME ".Pool",
    .basicsize = sizeof(PoolObject),
    .flags     = Py_TPFLAGS_DEFAULT,
    .slots     = Pool_slots,
};

type *PoolType = NULL;
