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
#ifndef ALITE_H
#define ALITE_H

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <sqlite3.h>

#define MODULE_NAME "_alite"

#define ALITE_DEFAULT_POOL_SIZE 4
#define ALITE_DEFAULT_BUSY_TIMEOUT "5000"

/* Rust style types */
typedef unsigned long      usize;
typedef signed char        i8;
typedef signed short       i16;
typedef signed long        i32;
typedef signed long long   i64;
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned long      u32;
typedef unsigned long long u64;
typedef float              f32;
typedef double             f64;

#endif // ALITE_H
