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
use pyo3::prelude::*;
use pyo3::types::{PyDict, PyList, PyTuple};
use rusqlite::types::Value as SqlValue;
use std::collections::HashMap;
use std::sync::Mutex;

use crate::connection::Connection;
use crate::row::Row;
use crate::value::sql_to_py;

pub enum ParamKind {
    Positional(Vec<SqlValue>),
    Named(Vec<(String, SqlValue)>),
}

#[pyclass(name = "Cursor")]
pub struct Cursor {
    pub stmt: Mutex<Option<rusqlite::Statement<'static>>>,
    pub conn: Connection,
    pub col_names: Mutex<Vec<String>>,
    pub col_index: Mutex<HashMap<String, usize>>,
    pub closed: Mutex<bool>,
    pub done: Mutex<bool>,
    pub rowcount: i64,
}

// SAFETY: all access to the inner statement is serialized through `stmt: Mutex<...>`,
// SAFETY: and the owning Connection is kept alive by `conn`, so cross thread use is sound.
unsafe impl Send for Cursor {}
unsafe impl Sync for Cursor {}

impl Cursor {
    pub fn new(stmt: rusqlite::Statement<'static>, conn: Connection, rowcount: i64) -> Self {
        Self {
            stmt: Mutex::new(Some(stmt)),
            conn,
            col_names: Mutex::new(Vec::new()),
            col_index: Mutex::new(HashMap::new()),
            closed: Mutex::new(false),
            done: Mutex::new(false),
            rowcount,
        }
    }

    pub fn empty(conn: Connection) -> Self {
        Self {
            stmt: Mutex::new(None),
            conn,
            col_names: Mutex::new(Vec::new()),
            col_index: Mutex::new(HashMap::new()),
            closed: Mutex::new(false),
            done: Mutex::new(true),
            rowcount: 0,
        }
    }

    pub fn with_rowcount(conn: Connection, rowcount: i64) -> Self {
        Self {
            stmt: Mutex::new(None),
            conn,
            col_names: Mutex::new(Vec::new()),
            col_index: Mutex::new(HashMap::new()),
            closed: Mutex::new(false),
            done: Mutex::new(true),
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
        if *self.closed.lock().unwrap() {
            return Err(pyo3::exceptions::PyRuntimeError::new_err(
                "Cursor is closed during fetchall",
            ));
        }
        let mut stmt_guard = self.stmt.lock().unwrap();
        let stmt = match stmt_guard.as_mut() {
            None => {
                drop(stmt_guard);
                return Ok(PyList::empty(py).unbind());
            }
            Some(s) => s,
        };
        if *self.done.lock().unwrap() {
            drop(stmt_guard);
            return Ok(PyList::empty(py).unbind());
        }

        if self.col_names.lock().unwrap().is_empty() {
            let names: Vec<String> = stmt.column_names().iter().map(|s| s.to_string()).collect();
            *self.col_names.lock().unwrap() = names.clone();
            let mut idx = HashMap::new();
            for (i, n) in names.iter().enumerate() {
                idx.entry(n.clone()).or_insert(i);
            }
            *self.col_index.lock().unwrap() = idx;
        }
        let ncols = stmt.column_count();
        let names = self.col_names.lock().unwrap().clone();

        drop(stmt_guard);

        let collected: Vec<Vec<SqlValue>> = py.detach(|| {
            let mut sg = self.stmt.lock().unwrap();
            let stmt = sg.as_mut().unwrap();
            let mut rows_iter = stmt.raw_query();
            let mut out = Vec::new();
            while let Some(row) = rows_iter
                .next()
                .map_err(|e| pyo3::exceptions::PyRuntimeError::new_err(e.to_string()))?
            {
                let mut vals = Vec::with_capacity(ncols);
                for i in 0..ncols {
                    let v: SqlValue = row.get(i).unwrap_or(SqlValue::Null);
                    vals.push(v);
                }
                out.push(vals);
            }
            Ok::<Vec<Vec<SqlValue>>, PyErr>(out)
        })?;

        *self.done.lock().unwrap() = true;

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

    fn close(&self) {
        let mut guard = self.stmt.lock().unwrap();
        if let Some(stmt) = guard.take() {
            let _ = stmt.finalize();
        }
        *self.closed.lock().unwrap() = true;
    }
}
