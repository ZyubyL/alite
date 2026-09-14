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
#include "bind.h"
#include "alite.h"

static inline i8 bind_none(sqlite3_stmt *s, i64 i)
{
    return sqlite3_bind_null(s, i);
}

static inline i8 bind_int(sqlite3_stmt *s, i64 i, PyObject *val)
{
    // Windows truncates PyLong_AsLongLong. So force it to be long long
    i64 v = PyLong_AsLongLong(val);
    if (v == -1 && PyErr_Occurred()) { return -1; }
    return sqlite3_bind_int64(s, i, (sqlite3_int64)v);
}

static inline i8 bind_float(sqlite3_stmt *s, i64 i, PyObject *v)
{
    return sqlite3_bind_double(s, i, PyFloat_AS_DOUBLE(v));
}

static inline i8 bind_str(sqlite3_stmt *s, i64 i, PyObject *v)
{
    Py_ssize_t len;
    const char *str = PyUnicode_AsUTF8AndSize(v, &len);
    if (!str) { return -1; }
    return sqlite3_bind_text(s, i, str, (i64)len, SQLITE_STATIC);
}

static inline i8 bind_bytes(sqlite3_stmt *s, i64 i, PyObject *v)
{
    return sqlite3_bind_blob(s, i, PyBytes_AS_STRING(v), (i64)PyBytes_GET_SIZE(v), SQLITE_STATIC);
}

i8 alite_bind_value(sqlite3_stmt *stmt, i64 index, PyObject *value)
{
    i8 rc;
    if (value == Py_None) { rc = bind_none(stmt, index); }
    else if (PyLong_Check(value)) { rc = bind_int(stmt, index, value); }
    else if (PyFloat_Check(value)) { rc = bind_float(stmt, index, value); }
    else if (PyUnicode_Check(value)) { rc = bind_str(stmt, index, value); }
    else if (PyBytes_Check(value)) { rc = bind_bytes(stmt, index, value); }
    else {
        PyErr_SetString(PyExc_TypeError, "Unsupported parameter type");
        return -1;
    }
    if (rc != SQLITE_OK && !PyErr_Occurred()) {
        PyErr_Format(
            PyExc_RuntimeError,
            "Bind failed: %s",
            sqlite3_errmsg(sqlite3_db_handle(stmt))
        );
    }
    return rc;
}

i8 alite_bind_positional(sqlite3_stmt *stmt, PyObject *params)
{
    PyObject *seq = PySequence_Fast(params, "params must be a sequence");
    if (!seq) { return -1; }

    Py_ssize_t len = PySequence_Fast_GET_SIZE(seq);
    for (Py_ssize_t i = 0; i < len; i++) {
        if (alite_bind_value(stmt, (i64)(i + 1), PySequence_Fast_GET_ITEM(seq, i)) != SQLITE_OK) {
            Py_DECREF(seq);
            return -1;
        }
    }
    Py_DECREF(seq);
    return SQLITE_OK;
}

i8 alite_bind_named(sqlite3_stmt *stmt, PyObject *params)
{
    Py_ssize_t pos = 0;
    PyObject *key, *value;
    while (PyDict_Next(params, &pos, &key, &value)) {
        const char *key_str = PyUnicode_AsUTF8(key);
        i64 idx = sqlite3_bind_parameter_index(stmt, key_str);
        if (idx == 0) {
            PyErr_Format(PyExc_ValueError, "Unknown parameter: %s", key_str);
            return -1;
        }
        if (alite_bind_value(stmt, idx, value) != SQLITE_OK) { return -1; }
    }
    return SQLITE_OK;
}
