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
#ifndef ALITE_ROW_H
#define ALITE_ROW_H

#include <Python.h>

typedef struct {
    PyObject_HEAD
    PyObject *values;
    PyObject *names;
    PyObject *index;
} RowObject;

extern PyTypeObject *RowType;
extern PyType_Spec   Row_spec;

/*
 * Build a Row. Steal all references.
 * Return NULL on failure with exception set.
 */
extern PyObject* Row_from_tuple(PyObject *values, PyObject *names, PyObject *index);

#endif // ALITE_ROW_H
