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
use pyo3::exceptions::PyRuntimeError;
use pyo3::prelude::*;
use pyo3::types::{PyDict, PyList, PyTuple};
use rusqlite::types::Value as SqlValue;
use std::collections::HashMap;
use std::sync::Mutex;

use crate::row::Row;
use crate::value::sql_to_py;

pub enum ParamKind {
    Positional(Vec<SqlValue>),
    Named(Vec<(String, SqlValue)>),
}

#[pyclass(name = "Cursor")]
pub struct Cursor {
    pub rows: Vec<Vec<SqlValue>>,
    pub col_names: Vec<String>,
    pub col_index: HashMap<String, usize>,
    pub pos: Mutex<usize>,
    pub done: Mutex<bool>,
    pub closed: Mutex<bool>,
    pub rowcount: i64,
}

impl Cursor {
    pub fn new(rows: Vec<Vec<SqlValue>>, col_names: Vec<String>, rowcount: i64) -> Self {
        let mut idx = HashMap::new();
        for (i, n) in col_names.iter().enumerate() {
            idx.entry(n.clone()).or_insert(i);
        }
        Self {
            rows,
            col_names,
            col_index: idx,
            pos: Mutex::new(0),
            done: Mutex::new(false),
            closed: Mutex::new(false),
            rowcount,
        }
    }

    pub fn empty() -> Self {
        Self {
            rows: Vec::new(),
            col_names: Vec::new(),
            col_index: HashMap::new(),
            pos: Mutex::new(0),
            done: Mutex::new(true),
            closed: Mutex::new(false),
            rowcount: 0,
        }
    }

    pub fn with_rowcount(rowcount: i64) -> Self {
        Self {
            rows: Vec::new(),
            col_names: Vec::new(),
            col_index: HashMap::new(),
            pos: Mutex::new(0),
            done: Mutex::new(true),
            closed: Mutex::new(false),
            rowcount,
        }
    }
}

#[pymethods]
impl Cursor {
    #[getter]
    fn rowcount(&self) -> i64 {
        self.rowcount
    }

    fn fetchall(&self, py: Python<'_>) -> PyResult<Py<PyList>> {
        if *self.closed.lock().unwrap_or_else(|e| e.into_inner()) {
            return Err(PyRuntimeError::new_err("Cursor is closed during fetchall"));
        }
        if *self.done.lock().unwrap_or_else(|e| e.into_inner()) {
            return Ok(PyList::empty(py).unbind());
        }

        let start = *self.pos.lock().unwrap_or_else(|e| e.into_inner());
        let names = self.col_names.clone();
        let collected: Vec<Vec<SqlValue>> = self.rows[start..].to_vec();

        *self.pos.lock().unwrap_or_else(|e| e.into_inner()) = self.rows.len();
        *self.done.lock().unwrap_or_else(|e| e.into_inner()) = true;

        let list = PyList::empty(py);
        for row_vals in collected {
            let py_vals: Vec<Py<PyAny>> =
                row_vals.iter().map(|v| sql_to_py(py, v).unwrap()).collect();
            let tuple = PyTuple::new(py, py_vals)?;
            let names_tuple = PyTuple::new(
                py,
                names
                    .iter()
                    .map(|n| n.clone().into_pyobject(py).unwrap().into_any().unbind())
                    .collect::<Vec<_>>(),
            )?;
            let index = PyDict::new(py);
            for (i, n) in names.iter().enumerate() {
                if !index.contains(n)? {
                    index.set_item(n, i)?;
                }
            }
            let row = Row::new(py, tuple.unbind(), names_tuple.unbind(), index.unbind())?;

            list.append(row)?;
        }

        Ok(list.unbind())
    }

    fn fetchone(&self, py: Python<'_>) -> PyResult<Option<Row>> {
        if *self.closed.lock().unwrap_or_else(|e| e.into_inner()) {
            return Err(PyRuntimeError::new_err("Cursor is closed during fetchone"));
        }
        if *self.done.lock().unwrap_or_else(|e| e.into_inner()) {
            return Ok(None);
        }

        let mut pos = self.pos.lock().unwrap_or_else(|e| e.into_inner());
        if *pos >= self.rows.len() {
            *self.done.lock().unwrap_or_else(|e| e.into_inner()) = true;
            return Ok(None);
        }

        let row_vals = self.rows[*pos].clone();
        *pos += 1;

        let finished = *pos >= self.rows.len();

        drop(pos);

        if finished {
            *self.done.lock().unwrap_or_else(|e| e.into_inner()) = true;
        }

        let names = self.col_names.clone();
        let py_vals: Vec<Py<PyAny>> = row_vals.iter().map(|v| sql_to_py(py, v).unwrap()).collect();
        let tuple = PyTuple::new(py, py_vals)?;
        let names_tuple = PyTuple::new(
            py,
            names
                .iter()
                .map(|n| n.clone().into_pyobject(py).unwrap().into_any().unbind())
                .collect::<Vec<_>>(),
        )?;
        let index = PyDict::new(py);
        for (i, n) in names.iter().enumerate() {
            if !index.contains(n)? {
                index.set_item(n, i)?;
            }
        }

        Ok(Some(Row::new(
            py,
            tuple.unbind(),
            names_tuple.unbind(),
            index.unbind(),
        )?))
    }

    fn fetchval(&self, py: Python<'_>) -> PyResult<Option<Py<PyAny>>> {
        if *self.closed.lock().unwrap_or_else(|e| e.into_inner()) {
            return Err(PyRuntimeError::new_err("Cursor is closed during fetchval"));
        }
        if *self.done.lock().unwrap_or_else(|e| e.into_inner()) {
            return Ok(None);
        }

        let mut pos = self.pos.lock().unwrap_or_else(|e| e.into_inner());
        if *pos >= self.rows.len() {
            *self.done.lock().unwrap_or_else(|e| e.into_inner()) = true;
            return Ok(None);
        }

        let value = if self.rows[*pos].is_empty() {
            None
        } else {
            Some(self.rows[*pos][0].clone())
        };

        *pos += 1;

        let finished = *pos >= self.rows.len();

        drop(pos);

        if finished {
            *self.done.lock().unwrap_or_else(|e| e.into_inner()) = true;
        }

        match value {
            Some(v) => Ok(Some(sql_to_py(py, &v)?)),
            None => Ok(None),
        }
    }

    fn close(&self) {
        *self.closed.lock().unwrap_or_else(|e| e.into_inner()) = true;
    }
}
