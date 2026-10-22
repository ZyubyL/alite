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
use pyo3::types::PyDict;
use std::sync::{Arc, Mutex};

use crate::connection::Connection;
use crate::cursor::{Cursor, ParamKind};
use crate::value::{named_params, positional_params};

struct PoolInner {
    path: String,
    pool_size: usize,
    connections: Vec<Connection>,
    opened: usize,
    closed: bool,
}

#[pyclass(name = "Pool")]
pub struct Pool {
    inner: Arc<Mutex<PoolInner>>,
}

impl Pool {
    fn get_connection(&self) -> PyResult<Connection> {
        let mut inner = self.inner.lock().unwrap();
        if inner.closed {
            return Err(pyo3::exceptions::PyRuntimeError::new_err("Pool is closed"));
        }
        for c in &inner.connections {
            let in_use = *c.in_use.lock().unwrap();
            let has_db = c.db.lock().unwrap().is_some();
            if !in_use && has_db {
                *c.in_use.lock().unwrap() = true;
                return Ok(c.clone());
            }
        }
        if inner.opened < inner.pool_size {
            let conn = Connection::open_db(&inner.path)?;
            *conn.in_use.lock().unwrap() = true;
            inner.connections.push(conn.clone());
            inner.opened += 1;
            return Ok(conn);
        }
        Err(pyo3::exceptions::PyRuntimeError::new_err(
            "Cannot get connection",
        ))
    }

    fn return_connection(&self, conn: &Connection) {
        *conn.in_use.lock().unwrap() = false;
    }

    fn execute_blocking(&self, sql: String, params: Option<ParamKind>) -> PyResult<Cursor> {
        let conn = self.get_connection()?;

        let mut stmt: rusqlite::Statement<'static> = {
            let guard = conn.db.lock().unwrap();
            let db = guard
                .as_ref()
                .ok_or_else(|| pyo3::exceptions::PyRuntimeError::new_err("Connection closed"))?;
            let s = db
                .prepare(&sql)
                .map_err(|e| pyo3::exceptions::PyRuntimeError::new_err(e.to_string()))?;
            unsafe { std::mem::transmute(s) }
        };

        match &params {
            Some(ParamKind::Positional(vals)) => {
                for (i, v) in vals.iter().enumerate() {
                    stmt.raw_bind_parameter(i + 1, v)
                        .map_err(|e| pyo3::exceptions::PyRuntimeError::new_err(e.to_string()))?;
                }
            }
            Some(ParamKind::Named(vals)) => {
                for (name, v) in vals {
                    let idx = stmt
                        .parameter_index(name)
                        .map_err(|e| pyo3::exceptions::PyRuntimeError::new_err(e.to_string()))?
                        .ok_or_else(|| {
                            pyo3::exceptions::PyValueError::new_err(format!(
                                "Unknown parameter: {}",
                                name
                            ))
                        })?;
                    stmt.raw_bind_parameter(idx, v)
                        .map_err(|e| pyo3::exceptions::PyRuntimeError::new_err(e.to_string()))?;
                }
            }
            None => {}
        }

        let rowcount: i64 = if stmt.readonly() {
            -1
        } else {
            let rc = stmt
                .raw_execute()
                .map_err(|e| pyo3::exceptions::PyRuntimeError::new_err(e.to_string()))?;
            rc as i64
        };

        let cursor = Cursor::new(stmt, conn.clone(), rowcount);
        self.return_connection(&conn);
        Ok(cursor)
    }

    fn executemany_blocking(&self, sql: String, rows: Vec<Option<ParamKind>>) -> PyResult<Cursor> {
        let conn = self.get_connection()?;
        if rows.is_empty() {
            self.return_connection(&conn);
            return Ok(Cursor::empty(conn));
        }

        let managed = {
            let guard = conn.db.lock().unwrap();
            let db = guard
                .as_ref()
                .ok_or_else(|| pyo3::exceptions::PyRuntimeError::new_err("Connection closed"))?;
            !db.is_autocommit()
        };
        if managed {
            let guard = conn.db.lock().unwrap();
            let db = guard.as_ref().unwrap();
            db.execute_batch("BEGIN IMMEDIATE")
                .map_err(|e| pyo3::exceptions::PyRuntimeError::new_err(e.to_string()))?;
        }

        let mut stmt: rusqlite::Statement<'static> = {
            let guard = conn.db.lock().unwrap();
            let db = guard
                .as_ref()
                .ok_or_else(|| pyo3::exceptions::PyRuntimeError::new_err("Connection closed"))?;
            match db.prepare(&sql) {
                Ok(s) => unsafe {
                    std::mem::transmute::<rusqlite::Statement<'_>, rusqlite::Statement<'_>>(s)
                },
                Err(e) => {
                    if managed {
                        let _ = db.execute_batch("ROLLBACK");
                    }
                    self.return_connection(&conn);
                    return Err(pyo3::exceptions::PyRuntimeError::new_err(e.to_string()));
                }
            }
        };

        let mut executed: i64 = 0;
        for row in &rows {
            stmt.clear_bindings();
            if let Some(Some(pk)) = Some(row) {
                match pk {
                    ParamKind::Positional(vals) => {
                        for (i, v) in vals.iter().enumerate() {
                            stmt.raw_bind_parameter(i + 1, v).map_err(|e| {
                                pyo3::exceptions::PyRuntimeError::new_err(e.to_string())
                            })?;
                        }
                    }
                    ParamKind::Named(vals) => {
                        for (name, v) in vals {
                            let idx = stmt
                                .parameter_index(name)
                                .map_err(|e| {
                                    pyo3::exceptions::PyRuntimeError::new_err(e.to_string())
                                })?
                                .ok_or_else(|| {
                                    pyo3::exceptions::PyValueError::new_err(format!(
                                        "Unknown parameter: {}",
                                        name
                                    ))
                                })?;
                            stmt.raw_bind_parameter(idx, v).map_err(|e| {
                                pyo3::exceptions::PyRuntimeError::new_err(e.to_string())
                            })?;
                        }
                    }
                }
            }
            if let Err(e) = stmt.raw_execute() {
                if managed {
                    let guard = conn.db.lock().unwrap();
                    if let Some(db) = guard.as_ref() {
                        let _ = db.execute_batch("ROLLBACK");
                    }
                }
                let _ = stmt.finalize();
                self.return_connection(&conn);
                return Err(pyo3::exceptions::PyRuntimeError::new_err(e.to_string()));
            }
            executed += 1;
        }

        let _ = stmt.finalize();

        if managed {
            let guard = conn.db.lock().unwrap();
            let db = guard.as_ref().unwrap();
            if let Err(e) = db.execute_batch("COMMIT") {
                self.return_connection(&conn);
                return Err(pyo3::exceptions::PyRuntimeError::new_err(e.to_string()));
            }
        }

        self.return_connection(&conn);
        Ok(Cursor::with_rowcount(conn, executed))
    }
}

#[pymethods]
impl Pool {
    #[new]
    #[pyo3(signature = (path, pool_size = 4))]
    fn new(path: String, pool_size: i64) -> PyResult<Self> {
        if !(1..=256).contains(&pool_size) {
            return Err(pyo3::exceptions::PyValueError::new_err(
                "pool_size must be between 1 and 256",
            ));
        }
        let pool_size = pool_size as usize;
        Ok(Self {
            inner: Arc::new(Mutex::new(PoolInner {
                path,
                pool_size,
                connections: Vec::new(),
                opened: 0,
                closed: false,
            })),
        })
    }

    #[getter]
    fn pool_size(&self) -> usize {
        self.inner.lock().unwrap().pool_size
    }

    pub fn close(&self) {
        let mut inner = self.inner.lock().unwrap();
        inner.closed = true;
        for c in &inner.connections {
            c.close();
        }
    }

    #[pyo3(signature = (sql, params = None))]
    fn execute(&self, py: Python<'_>, sql: String, params: Option<Py<PyAny>>) -> PyResult<Cursor> {
        // convert params on the GIL side
        let kind = if let Some(p) = params {
            let bound = p.bind(py);
            if bound.is_none() {
                None
            } else if let Ok(dict) = bound.cast::<PyDict>() {
                Some(ParamKind::Named(named_params(py, dict)?))
            } else {
                positional_params(py, Some(bound))?.map(ParamKind::Positional)
            }
        } else {
            None
        };
        py.detach(|| self.execute_blocking(sql, kind))
    }

    fn executemany(&self, py: Python<'_>, sql: String, params_list: Py<PyAny>) -> PyResult<Cursor> {
        let bound = params_list.bind(py);
        let mut rows: Vec<Option<ParamKind>> = Vec::new();
        if let Ok(iter) = bound.try_iter() {
            for item in iter {
                let item = item?;
                if item.is_none() {
                    rows.push(None);
                    continue;
                }
                if let Ok(dict) = item.cast::<PyDict>() {
                    rows.push(Some(ParamKind::Named(named_params(py, dict)?)));
                } else {
                    let pk = positional_params(py, Some(&item))?.map(ParamKind::Positional);
                    rows.push(pk);
                }
            }
        }
        py.detach(|| self.executemany_blocking(sql, rows))
    }
}

impl Drop for Pool {
    fn drop(&mut self) {
        if let Ok(mut inner) = self.inner.lock() {
            inner.closed = true;
            for c in &inner.connections {
                c.close();
            }
        }
    }
}
