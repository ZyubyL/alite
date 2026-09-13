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

static i8 Cursor_init(CursorObject *self, PyObject *args, PyObject *kwargs)
{
    return 0;
}

static void Cursor_dealloc(CursorObject *self)
{
    Py_XDECREF(self->conn);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject* Cursor_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    CursorObject *self = (CursorObject *)type->tp_alloc(type, 0);
    if (self) {
        self->conn = nullptr;
    }
    return (PyObject *)self;
}

PyTypeObject CursorType = {
    PyVarObject_HEAD_INIT(nullptr, 0)
    .tp_name      = MODULE_NAME ".Connection",
    .tp_basicsize = sizeof(CursorObject),
    .tp_doc       = "SQLite query cursor",
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_init      = (initproc)Cursor_init,
    .tp_del       = (destructor)Cursor_dealloc,
    .tp_new       = Cursor_new,
};
