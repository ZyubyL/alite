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

static inline int bind_none(sqlite3_stmt *s, int i)
{
    return sqlite3_bind_null(s, (int)i);
}

static inline int bind_int(sqlite3_stmt *s, int i, object *val)
{
    // Windows truncates PyLong_AsLongLong. So force it to be long long
    i64 v = PyLong_AsLongLong(val);
    if (v == -1 and PyErr_Occurred()) { return -1; }
    return sqlite3_bind_int64(s, i, (sqlite3_int64)v);
}

static inline int bind_float(sqlite3_stmt *s, int i, object *v)
{
    return sqlite3_bind_double(s, i, PyFloat_AS_DOUBLE(v));
}

static inline int bind_str(sqlite3_stmt *s, int i, object *v)
{
    isize len;
    const char *str = PyUnicode_AsUTF8AndSize(v, &len);
    if (not str) { return -1; }
    return sqlite3_bind_text(s, i, str, (int)len, SQLITE_STATIC);
}

static inline int bind_bytes(sqlite3_stmt *s, int i, object *v)
{
    return sqlite3_bind_blob(s, i, PyBytes_AS_STRING(v), PyBytes_GET_SIZE(v), SQLITE_STATIC);
}

int alite_bind_value(sqlite3_stmt *stmt, int index, object *value)
{
    int rc;
    if (value == None)               { rc = bind_none(stmt, index);         }
    else if (PyLong_Check(value))    { rc = bind_int(stmt, index, value);   }
    else if (PyFloat_Check(value))   { rc = bind_float(stmt, index, value); }
    else if (PyUnicode_Check(value)) { rc = bind_str(stmt, index, value);   }
    else if (PyBytes_Check(value))   { rc = bind_bytes(stmt, index, value); }
    else {
        PyErr_SetString(TypeError, "Unsupported parameter type");
        return -1;
    }
    if (rc != SQLITE_OK and not PyErr_Occurred()) {
        PyErr_Format(
            RuntimeError,
            "Bind failed: %s",
            sqlite3_errmsg(sqlite3_db_handle(stmt))
        );
    }
    return rc;
}

int alite_bind_positional(sqlite3_stmt *stmt, object *params)
{
    object *seq = PySequence_Fast(params, "params must be a sequence");
    if (not seq) { return -1; }

    isize len = PySequence_Fast_GET_SIZE(seq);
    for (isize i = 0; i < len; i++) {
        if (alite_bind_value(stmt, (int)(i + 1), PySequence_Fast_GET_ITEM(seq, i)) != SQLITE_OK) {
            Py_DECREF(seq);
            return -1;
        }
    }
    Py_DECREF(seq);
    return SQLITE_OK;
}

int alite_bind_named(sqlite3_stmt *stmt, object *params)
{
    isize pos = 0;
    object *key, *value;
    while (PyDict_Next(params, &pos, &key, &value)) {
        const char *key_str = PyUnicode_AsUTF8(key);
        int idx = sqlite3_bind_parameter_index(stmt, key_str);
        if (idx == 0) {
            PyErr_Format(ValueError, "Unknown parameter: %s", key_str);
            return -1;
        }
        if (alite_bind_value(stmt, idx, value) != SQLITE_OK) { return -1; }
    }
    return SQLITE_OK;
}
