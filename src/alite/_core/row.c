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
#include "row.h"
#include "alite.h"

object* Row_from_tuple(object *values, object *names, object *index)
{
    RowObject *self = PyObject_New(RowObject, RowType);
    if (not self) {
        Py_DECREF(values);
        Py_DECREF(names);
        Py_DECREF(index);
        return NULL;
    }
    self->values = values;
    self->names = names;
    self->index = index;
    return (object *)self;
}

static i8 validate_key_type(object *key)
{
    if (PyUnicode_Check(key) || PyLong_Check(key)) { return 0; }
    PyErr_SetString(TypeError, "Row indices must be int or str");
    return -1;
}

static object* lookup_str_index(RowObject *self, object *key)
{
    object *idx = PyDict_GetItemWithError(self->index, key);
    if (not idx) {
        if (not PyErr_Occurred()) {
            PyErr_SetString(IndexError, "No item with that key");
        }
        return NULL;
    }
    return idx;
}

static i8 convert_index_to_size_t(object *idx, isize *result)
{
    const isize i = PyLong_AsSsize_t(idx);
    if (i == -1 and PyErr_Occurred()) { return -1; }
    *result = i;
    return 0;
}

static object* get_row_val(RowObject *self, isize index)
{
    return PySequence_GetItem(self->values, index);
}

/* row[0], row["name"] */
static object* Row_subscript(RowObject *self, object *key)
{
    if (validate_key_type(key) != 0) { return NULL; }

    object *idx = key;

    if (PyUnicode_Check(key)) {
        idx = lookup_str_index(self, key);
        if (not idx) { return NULL; }
    }

    isize index;
    if (convert_index_to_size_t(idx, &index) != 0) { return NULL; }

    return get_row_val(self, index);
}

static object* Row_item(RowObject *self, isize i)
{
    object *v = PyTuple_GetItem(self->values, i);
    Py_XINCREF(v);
    return v;
}

static isize Row_length(RowObject *self)
{
    return PyTuple_GET_SIZE(self->values);
}

static object* Row_iter(RowObject *self)
{
    return PyObject_GetIter(self->values);
}

/* row.keys() -> list[str] */
static object* Row_keys(RowObject *self, object *Py_UNUSED(ignored))
{
    return PySequence_List(self->names);
}

static object* Row_repr(RowObject *self)
{
    return PyUnicode_FromFormat("<alite.Row %R>", self->values);
}

/* EQ/NE against tuple and Row. */
static object* Row_richcompare(RowObject *self, object *other, int op)
{
    if (op != Py_EQ and op != Py_NE) { Py_RETURN_NOTIMPLEMENTED; }

    object *tup;
    if (Py_TYPE(other) == RowType) { tup = ((RowObject *)other)-> values; }
    else if (PyTuple_Check(other)) { tup = other; }
    else { Py_RETURN_NOTIMPLEMENTED; }

    Py_INCREF(tup);
    object *res = PyObject_RichCompare(self->values, tup, op);
    Py_DECREF(tup);
    return res;
}

static void Row_dealloc(RowObject *self)
{
    Py_XDECREF(self->values);
    Py_XDECREF(self->names);
    Py_XDECREF(self->index);
    FREE_OBJ;
}

/* Block Row() construction from Python */
static object* Row_new_impl(type *type, object *args, object *kwargs)
{
    PyErr_SetString(TypeError, "Cannot create Row directly");
    return NULL;
}

static PyMethodDef Row_methods[] = {
    { "keys", (PyCFunction)Row_keys, METH_NOARGS, "Column names" },
    { NULL },
};

static PyType_Slot Row_slots[] = {
    { Py_tp_doc,         "SQLite result row with index and key access" },
    { Py_tp_new,         Row_new_impl                                  },
    { Py_tp_dealloc,     Row_dealloc                                   },
    { Py_tp_methods,     Row_methods                                   },
    { Py_tp_repr,        Row_repr                                      },
    { Py_tp_richcompare, Row_richcompare                               },
    { Py_tp_iter,        Row_iter                                      },
    { Py_mp_subscript,   Row_subscript                                 },
    { Py_sq_item,        Row_item                                      },
    { Py_sq_length,      Row_length                                    },
    { 0,                 NULL                                          },
};

PyType_Spec Row_spec = {
    .name      = MODULE_NAME ".Row",
    .basicsize = sizeof(RowObject),
    .flags     = Py_TPFLAGS_DEFAULT,
    .slots     = Row_slots,
};

type *RowType = NULL;
