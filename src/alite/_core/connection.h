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
#ifndef ALITE_CONNECTION_H
#define ALITE_CONNECTION_H

#include "alite.h"

typedef struct {
    PyObject_HEAD
    sqlite3 *db;
    i8       in_use;
    usize    open_stmts;
} ConnectionObject;

extern PyTypeObject ConnectionType;

/*
 * Open the database connection at path.
 * Enable WAL mode and set busy_timeout to 5000.
 * Return 0 on success, -1 on failure (self->db remains NULL on failure).
 */
extern i8 Connection_open_db(ConnectionObject *self, const char *path);

/*
 * Close the database. Release GIL during the close call.
 * Return SQLITE_OK or SQLITE_BUSY.
 * Set self->db to NULL on success.
 */
extern i8 Connection_close_db(ConnectionObject *self);

#endif // ALITE_CONNECTION_H
