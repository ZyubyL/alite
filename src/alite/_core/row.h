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

#include "alite.h"

typedef struct {
    PyObject_HEAD
    object *values;
    object *names;
    object *index;
} RowObject;

extern type       *RowType;
extern PyType_Spec Row_spec;

/*
 * Build a Row. Steal all references.
 * Return NULL on failure with exception set.
 */
extern object* Row_from_tuple(object *values, object *names, object *index);

#endif // ALITE_ROW_H
