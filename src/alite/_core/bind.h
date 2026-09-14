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
#ifndef ALITE_BIND_H
#define ALITE_BIND_H

#include "alite.h"

/*
 * Bind single value to statement at index.
 * Return SQLITE_OK on success, none zero on error + exception.
 */
extern i8 alite_bind_value(sqlite3_stmt *stmt, i64 index, PyObject *value);

/*
 * Bind positional params (tuple/list) to statement.
 * Return SQLITE_OK on success, -1 on error.
 */
extern i8 alite_bind_positional(sqlite3_stmt *stmt, PyObject *params);

/*
 * Bind named params (dict) to statement
 * Return SQLITE_OK on success, -1 on error.
 */
extern i8 alite_bind_named(sqlite3_stmt *stmt, PyObject *params);

#endif // ALITE_BIND_H
