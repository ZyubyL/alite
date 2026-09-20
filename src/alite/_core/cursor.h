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
#ifndef ALITE_CURSOR_H
#define ALITE_CURSOR_H

#include "alite.h"
#include "connection.h"

typedef struct {
    PyObject_HEAD
    sqlite3_stmt     *stmt;
    ConnectionObject *conn;
    i8                closed;
    i64               rowcount;
} CursorObject;

extern PyTypeObject *CursorType;
extern PyType_Spec   Cursor_spec;

/*
 * Prepare and bind an SQL statement. Increase conn->open_stmts on success.
 * Release GIL during preparation.
 * Return 0 on success, -1 on failure.
 */
extern i8 Cursor_prepare(CursorObject *self, ConnectionObject *conn, const char *sql, PyObject *params);

#endif // ALITE_CURSOR_H
