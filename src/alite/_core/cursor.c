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
#include "row.h"
#include <sqlite3.h>


/* Bind params to stmt */
static inline i8 Cursor_bind_params(CursorObject *self, object *params)
{
    if (not params or params == None) { return 0; }

    sqlite3_reset(self->stmt);
    sqlite3_clear_bindings(self->stmt);

    if (PyTuple_Check(params) or PyList_Check(params)) {
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

/* Column i of current row as Python object. NULL on error. */
static inline object* column_value(sqlite3_stmt *stmt, int col)
{
    switch (sqlite3_column_type(stmt, col)) {
        case SQLITE_INTEGER:
            return PyLong_FromLongLong(sqlite3_column_int64(stmt, col));
        case SQLITE_FLOAT:
            return PyFloat_FromDouble(sqlite3_column_double(stmt, col));
        case SQLITE_TEXT: {
            const char *text = (const char *)sqlite3_column_text(stmt, col);
            const int len = sqlite3_column_bytes(stmt, col);
            return PyUnicode_FromStringAndSize(text, len);
        }
        case SQLITE_BLOB: {
            const void *blob = sqlite3_column_blob(stmt, col);
            const int len = sqlite3_column_bytes(stmt, col);
            return PyBytes_FromStringAndSize(blob, len);
        }
        case SQLITE_NULL:
        default:
            Py_RETURN_NONE;
    }
}

/* Build col_names + col_index once per statement. Keep first if have duplicate. */
static i8 ensure_columns(CursorObject *self)
{
    if (self->col_names) { return 0; }

    const int ncols = sqlite3_column_count(self->stmt);
    object *names = PyTuple_New(ncols);
    object *index = PyDict_New();
    if (not names or not index) { goto failure; }

    for (int i = 0; i < ncols; i++) {
        const char *cname = sqlite3_column_name(self->stmt, i);
        if (not cname) {
            PyErr_NoMemory();
            goto failure;
        }
        object *name = PyUnicode_FromString(cname);
        if (not name) { goto failure; }
        PyTuple_SET_ITEM(names, i, name);

        if (PyDict_GetItemWithError(index, name) == NULL) {
            if (PyErr_Occurred()) { goto failure; }
            object *idx = PyLong_FromLong(i);
            if (not idx) { goto failure; }
            int rc = PyDict_SetItem(index, name, idx);
            Py_DECREF(idx);
            if (rc < 0) { goto failure; }
        }
    }
    self->col_names = names;
    self->col_index = index;
    return 0;

failure:
    Py_XDECREF(names);
    Py_XDECREF(index);
    return -1;
}

/* Build Row from current stmt row. */
static object* make_row(CursorObject *self, int ncols)
{
    object *values = PyTuple_New(ncols);
    if (not values) { return NULL; }
    for (int i = 0; i < ncols; i++) {
        object *v = column_value(self->stmt, i);
        if (not v) {
            Py_DECREF(values);
            return NULL;
        }
        PyTuple_SET_ITEM(values, i, v);
    }
    Py_INCREF(self->col_names);
    Py_INCREF(self->col_index);
    return Row_from_tuple(values, self->col_names, self->col_index);
}

static inline void clear_columns(CursorObject *self)
{
    Py_CLEAR(self->col_names);
    Py_CLEAR(self->col_index);
}

/* Finalize the statement, set it to NULL, decrease opening stmt */
static void finalize_stmt(CursorObject *self, ConnectionObject *conn)
{
    sqlite3_finalize(self->stmt);
    clear_columns(self);
    MAKE_NULL(self->stmt);
    if (conn) { conn->open_stmts--; }
}

i8 Cursor_prepare(CursorObject *self, ConnectionObject *conn, const char *sql, object *params)
{
    Py_INCREF(conn);
    self->conn = conn;
    // If there is still a statement that is not yet finalized. Finalize it.
    if (self->stmt) {
        sqlite3_finalize(self->stmt);
        clear_columns(self);
        MAKE_NULL(self->stmt);
    };

    if (do_prepare(self, conn, sql) != SQLITE_OK) {
        PyErr_Format(RuntimeError, "Prepare failed: %s", sqlite3_errmsg(conn->db));
        return -1;
    }

    self->done = 0;
    if (conn) { conn->open_stmts++; }
    if (Cursor_bind_params(self, params) != 0) {
        finalize_stmt(self, conn);
        return -1;
    }

    if (sqlite3_stmt_readonly(self->stmt)) {
        self->rowcount = -1;
    } else {
        stmt_step_gil(self->stmt);
        self->rowcount = sqlite3_changes(conn->db);
    }
    return 0;
}

static object* Cursor_get_rowcount(CursorObject *self, void *closure)
{
    return PyLong_FromLongLong(self->rowcount);
}

static object* Cursor_fetchall(CursorObject *self, object* Py_UNUSED(ignored))
{
    if (self->closed) {
        PyErr_SetString(RuntimeError, "Cursor is closed");
        return NULL;
    }
    if (not self->stmt or self->done) { return PyList_New(0); }
    if (ensure_columns(self) != 0) { return NULL; }

    object *rows = PyList_New(0);
    if (not rows) { return NULL; }

    int ncols = sqlite3_column_count(self->stmt);
    while (1) {
        i8 rc = stmt_step_gil(self->stmt);

        if (rc == SQLITE_DONE) {
            self->done = 1;
            break;
        }
        if (rc != SQLITE_ROW) {
            Py_DECREF(rows);
            PyErr_Format(RuntimeError, "Step failed: %s", errmsg_from_stmt(self->stmt));
            return NULL;
        }
        object *row = make_row(self, ncols);
        if (not row or PyList_Append(rows, row) < 0) {
            Py_XDECREF(row);
            Py_DECREF(rows);
            return NULL;
        }
        Py_DECREF(row);
    }
    return rows;
}

static object* Cursor_close(CursorObject *self, object *args)
{
    if (not self->closed and self->stmt) {
        finalize_stmt(self, self->conn);
        self->closed = 1;
    }
    Py_RETURN_NONE;
}

static int Cursor_init(CursorObject *self, object *args, object *kwargs)
{
    return 0;
}

static void Cursor_dealloc(CursorObject *self)
{
    if (not self->closed and self->stmt) {
        finalize_stmt(self, self->conn);
        self->closed = 1;
    }
    clear_columns(self);
    Py_XDECREF(self->conn);
    FREE_OBJ;
}

static object* Cursor_new(type *type, object *args, object *kwargs)
{
    CursorObject *self = (CursorObject *)type->tp_alloc(type, 0);
    if (self) {
        MAKE_NULL(self->conn);
        MAKE_NULL(self->stmt);
        MAKE_NULL(self->col_names);
        MAKE_NULL(self->col_index);
        self->closed = 0;
        self->done = 0;
        self->rowcount = 0;
    }
    return (object *)self;
}

static PyMethodDef Cursor_methods[] = {
    { "fetchall", (PyCFunction)Cursor_fetchall, METH_NOARGS, "Fetch all remaining rows" },
    { "close", (PyCFunction)Cursor_close, METH_NOARGS, "Close the cursor" },
    { NULL },
};

static PyGetSetDef Cursor_getset[] = {
    { "rowcount", (getter)Cursor_get_rowcount, NULL, "Number of rows affected", NULL },
    { NULL },
};

static PyType_Slot Cursor_slots[] = {
    { Py_tp_doc,     "SQLite query cursor" },
    { Py_tp_init,    Cursor_init           },
    { Py_tp_new,     Cursor_new            },
    { Py_tp_dealloc, Cursor_dealloc        },
    { Py_tp_getset,  Cursor_getset         },
    { Py_tp_methods, Cursor_methods        },
    { 0,             NULL                  },
};

PyType_Spec Cursor_spec = {
    .name      = MODULE_NAME ".Cursor",
    .basicsize = sizeof(CursorObject),
    .flags     = Py_TPFLAGS_DEFAULT,
    .slots     = Cursor_slots,
};

type *CursorType = NULL;
