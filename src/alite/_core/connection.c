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

static i8 Connection_init(ConnectionObject *self, PyObject *args, PyObject *kwargs)
{
    return 0;
}

static void Connection_dealloc(ConnectionObject *self)
{
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject* Connection_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    ConnectionObject *self = (ConnectionObject *)type->tp_alloc(type, 0);
    if (self) { self->db = NULL; }
    return (PyObject *)self;
}

PyTypeObject ConnectionType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = MODULE_NAME ".Pool",
    .tp_basicsize = sizeof(ConnectionObject),
    .tp_doc       = "SQLite connection",
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_init      = (initproc)Connection_init,
    .tp_dealloc   = (destructor)Connection_dealloc,
    .tp_new       = Connection_new,
};
