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

void Connection_free(ConnectionObject *self) {
    if (self->in_use) {
        self->in_use = 0;
        free(self);
    }
}

/* Close the database. */
static int do_close(ConnectionObject *self)
{
    if (not self->db) { return SQLITE_OK; }
    int rc = sqlite3_close_v2(self->db);
    if (rc == SQLITE_OK) { MAKE_NULL(self->db); }
    return rc;
}

/* Open the database at path. Release GIL when doing. */
static inline int open_db(ConnectionObject *self, const char *path)
{
    int rc;
    Py_BEGIN_ALLOW_THREADS
    rc = sqlite3_open(path, &self->db);
    Py_END_ALLOW_THREADS
    return rc;
}

/* Enable WAL mode and set busy timeout. Release GIL when doing. */
static inline int enable_wal(ConnectionObject *self)
{
    int rc;
    Py_BEGIN_ALLOW_THREADS
    rc = sqlite3_exec(self->db, "PRAGMA journal_mode=WAL", NULL, NULL, NULL);
    if (rc == SQLITE_OK) {
        rc = sqlite3_exec(self->db, "PRAGMA busy_timeout=" ALITE_DEFAULT_BUSY_TIMEOUT, NULL, NULL, NULL);
    }
    Py_END_ALLOW_THREADS
    return rc;
}

static i8 validate_connection_state(ConnectionObject *self)
{
    if (self->db) {
        PyErr_SetString(RuntimeError, "Connection already opened");
        return -1;
    }
    return 0;
}

static void handle_connection_error(ConnectionObject *self, const char *op)
{
    if (self->db) {
        PyErr_Format(ConnectionError, "Failed to %s database: %s", op, sqlite3_errmsg(self->db));
        do_close(self);
    }
}

static i8 config_db_settings(ConnectionObject *self)
{
    if (enable_wal(self) != SQLITE_OK) {
        handle_connection_error(self, "config");
        return -1;
    }
    return 0;
}

i8 Connection_open_db(ConnectionObject *self, const char *path)
{
    if (validate_connection_state(self) != 0) {
        return -1;
    }

    if (open_db(self, path) != SQLITE_OK) {
        handle_connection_error(self, "open");
        return -1;
    }

    return config_db_settings(self);
}

int Connection_close_db(ConnectionObject *self)
{
    if (not self->db) { return SQLITE_OK; }
    int rc;
    Py_BEGIN_ALLOW_THREADS
    rc = do_close(self);
    Py_END_ALLOW_THREADS
    return rc;
}

static object* Connection_close(ConnectionObject *self, object *Py_UNUSED(ignored))
{
    Connection_close_db(self);
    Py_RETURN_NONE;
}

static int Connection_init(ConnectionObject *self, object *args, object *kwargs)
{
    return 0;
}

static void Connection_dealloc(ConnectionObject *self)
{
    do_close(self);
    FREE_OBJ;
}

static object* Connection_new(type *type, object *args, object *kwargs)
{
    ConnectionObject *self = (ConnectionObject *)type->tp_alloc(type, 0);
    if (self) { MAKE_NULL(self->db); }
    return (object *)self;
}

static PyMethodDef Connection_methods[] = {
    { "close", (PyCFunction)Connection_close, METH_NOARGS, "Close database connection" },
    { NULL },
};

static PyType_Slot Connection_slots[] = {
    { Py_tp_doc,     "SQLite connection" },
    { Py_tp_init,    Connection_init     },
    { Py_tp_new,     Connection_new      },
    { Py_tp_dealloc, Connection_dealloc  },
    { Py_tp_methods, Connection_methods  },
    { 0,             NULL                },
};

PyType_Spec Connection_spec = {
    .name      = MODULE_NAME ".Connection",
    .basicsize = sizeof(ConnectionObject),
    .flags     = Py_TPFLAGS_DEFAULT,
    .slots     = Connection_slots,
};

type *ConnectionType = NULL;
