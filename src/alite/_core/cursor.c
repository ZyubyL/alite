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
#include "cursor.h"
#include "alite.h"
#include "bind.h"
#include "connection.h"


/* Bind params to stmt */
static inline i8 Cursor_bind_params(CursorObject *self, PyObject *params)
{
    if (!params || params == Py_None) { return 0; }

    sqlite3_reset(self->stmt);
    sqlite3_clear_bindings(self->stmt);

    if (PyTuple_Check(params) || PyList_Check(params)) {
        return alite_bind_positional(self->stmt, params);
    } else if (PyDict_Check(params)) {
        return alite_bind_named(self->stmt, params);
    }
    return 0;
}

/* Prepare the statement. Release GIL while doing. */
static inline i8 do_prepare(CursorObject *self, ConnectionObject *conn, const char *sql)
{
    i8 rc;
    Py_BEGIN_ALLOW_THREADS
    rc = sqlite3_prepare_v2(conn->db, sql, -1, &self->stmt, NULL);
    Py_END_ALLOW_THREADS
    return rc;
}

i8 Cursor_prepare(CursorObject *self, ConnectionObject *conn, const char *sql, PyObject *params)
{
    Py_INCREF(conn);
    self->conn = conn;
    // If there is still a statement that is not yet finalized. Finalize it.
    if (self->stmt) {
        sqlite3_finalize(self->stmt);
        self->stmt = NULL;
    };

    if (do_prepare(self, conn, sql) != SQLITE_OK) {
        PyErr_Format(PyExc_RuntimeError, "Prepare failed: %s", sqlite3_errmsg(conn->db));
        return -1;
    }

    if (conn) { conn->open_stmts++; }
    if (Cursor_bind_params(self, params) != 0) {
        sqlite3_finalize(self->stmt);
        self->stmt = NULL;
        if (conn) conn->open_stmts--;
        return -1;
    }

    if (sqlite3_stmt_readonly(self->stmt)) {
        self->rowcount = -1;
    } else {
        Py_BEGIN_ALLOW_THREADS
        sqlite3_step(self->stmt);
        Py_END_ALLOW_THREADS
        self->rowcount = sqlite3_changes(conn->db);
    }
    return 0;
}

static i8 Cursor_init(CursorObject *self, PyObject *args, PyObject *kwargs)
{
    return 0;
}

static void Cursor_dealloc(CursorObject *self)
{
    Py_XDECREF(self->conn);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject* Cursor_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    CursorObject *self = (CursorObject *)type->tp_alloc(type, 0);
    if (self) {
        self->conn = NULL;
        self->closed = 0;
    }
    return (PyObject *)self;
}

PyTypeObject CursorType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = MODULE_NAME ".Cursor",
    .tp_basicsize = sizeof(CursorObject),
    .tp_doc       = "SQLite query cursor",
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_init      = (initproc)Cursor_init,
    .tp_dealloc   = (destructor)Cursor_dealloc,
    .tp_new       = Cursor_new,
};
