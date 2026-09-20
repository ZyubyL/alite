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
#include "connection.h"
#include "alite.h"

/* Close the database. */
static i8 do_close(ConnectionObject *self)
{
    if (!self->db) { return SQLITE_OK; }
    i8 rc = sqlite3_close_v2(self->db);
    if (rc == SQLITE_OK) { self->db = NULL; }
    return rc;
}

/* Open the database at path. Release GIL when doing. */
static inline i8 open_db(ConnectionObject *self, const char *path)
{
    i8 rc;
    Py_BEGIN_ALLOW_THREADS
    rc = sqlite3_open(path, &self->db);
    Py_END_ALLOW_THREADS
    return rc;
}

/* Enable WAL mode and set busy timeout. Release GIL when doing. */
static inline i8 enable_wal(ConnectionObject *self)
{
    i8 rc;
    Py_BEGIN_ALLOW_THREADS
    rc = sqlite3_exec(self->db, "PRAGMA journal_mode=WAL", NULL, NULL, NULL);
    if (rc == SQLITE_OK) {
        rc = sqlite3_exec(self->db, "PRAGMA busy_timeout=" ALITE_DEFAULT_BUSY_TIMEOUT, NULL, NULL, NULL);
    }
    Py_END_ALLOW_THREADS
    return rc;
}

i8 Connection_open_db(ConnectionObject *self, const char *path)
{
    if (self->db) {
        PyErr_SetString(PyExc_RuntimeError, "Connection already opened");
        return -1;
    }
    if (open_db(self, path) != SQLITE_OK) {
        PyErr_Format(PyExc_ConnectionError, "Failed to open database: %s", sqlite3_errmsg(self->db));
        do_close(self);
        return -1;
    }
    if (enable_wal(self) != SQLITE_OK) {
        PyErr_Format(PyExc_ConnectionError, "Failed to config the database: %s", sqlite3_errmsg(self->db));
        do_close(self);
        return -1;
    }
    return 0;
}

i8 Connection_close_db(ConnectionObject *self)
{
    if (!self->db) { return SQLITE_OK; }
    i8 rc;
    Py_BEGIN_ALLOW_THREADS
    rc = do_close(self);
    Py_END_ALLOW_THREADS
    return rc;
}

static PyObject* Connection_close(ConnectionObject *self, PyObject *Py_UNUSED(ignored))
{
    Connection_close_db(self);
    Py_RETURN_NONE;
}

static int Connection_init(ConnectionObject *self, PyObject *args, PyObject *kwargs)
{
    return 0;
}

static void Connection_dealloc(ConnectionObject *self)
{
    do_close(self);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject* Connection_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    ConnectionObject *self = (ConnectionObject *)type->tp_alloc(type, 0);
    if (self) { self->db = NULL; }
    return (PyObject *)self;
}

static PyMethodDef Connection_methods[] = {
    { "close", (PyCFunction)Connection_close, METH_NOARGS, "Close database connection" },
    { NULL },
};

static PyType_Slot Connection_slots[] = {
    {Py_tp_doc, "SQLite connection"},
    {Py_tp_init, Connection_init},
    {Py_tp_new, Connection_new},
    {Py_tp_dealloc, Connection_dealloc},
    {Py_tp_methods, Connection_methods},
    {0, NULL},
};

PyType_Spec Connection_spec = {
    .name      = MODULE_NAME ".Connection",
    .basicsize = sizeof(ConnectionObject),
    .flags     = Py_TPFLAGS_DEFAULT,
    .slots     = Connection_slots,
};

PyTypeObject *ConnectionType = NULL;
