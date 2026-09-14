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
#ifndef ALITE_POOL_H
#define ALITE_POOL_H

#include "alite.h"
#include "connection.h"

typedef struct {
    PyObject_HEAD
    ConnectionObject **connections;
    i64                opened_conns;
    char              *path;
    i64                pool_size;
    PyThread_type_lock lock;
    i8                 lock_init;
    i8                 closed;
} PoolObject;

extern PyTypeObject PoolType;

/*
 * Checkout a connection from the pool. Create a new one if under pool_size.
 * Set conn->in_use to 1. Must call Pool_return_connection when done.
 * Return NULL with exception on failure.
 */
ConnectionObject* Pool_get_connection(PoolObject *self);

/*
 * Return the connection to the pool.
 * Set conn->in_use to 0.
 */
void Pool_return_connection(PoolObject *self, ConnectionObject *conn);

#endif //ALITE_POOL_H
