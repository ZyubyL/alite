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
use pyo3::exceptions::{PyRuntimeError, PyValueError};
use pyo3::prelude::*;
use pyo3::types::PyDict;
use rusqlite::types::Value as SqlValue;
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

struct ConnGuard<'a> {
    pool: &'a Pool,
    conn: Connection,
}

impl Drop for ConnGuard<'_> {
    fn drop(&mut self) {
        self.pool.return_connection(&self.conn);
    }
}

impl Pool {
    fn get_connection(&self) -> PyResult<Connection> {
        let mut inner = self.inner.lock().unwrap_or_else(|e| e.into_inner());
        if inner.closed {
            return Err(PyRuntimeError::new_err("Pool is closed"));
        }

        for c in &inner.connections {
            let in_use = *c.in_use.lock().unwrap_or_else(|e| e.into_inner());
            let has_db = c.db.lock().unwrap_or_else(|e| e.into_inner()).is_some();
            if !in_use && has_db {
                *c.in_use.lock().unwrap_or_else(|e| e.into_inner()) = true;
                return Ok(c.clone());
            }
        }

        if inner.opened < inner.pool_size {
            let conn = Connection::open_db(&inner.path)?;
            *conn.in_use.lock().unwrap_or_else(|e| e.into_inner()) = true;
            inner.connections.push(conn.clone());
            inner.opened += 1;
            return Ok(conn);
        }

        Err(pyo3::exceptions::PyRuntimeError::new_err(
            "Cannot get connection",
        ))
    }

    fn return_connection(&self, conn: &Connection) {
        *conn.in_use.lock().unwrap_or_else(|e| e.into_inner()) = false;
    }

    fn bind_params(stmt: &mut rusqlite::Statement<'_>, params: &Option<ParamKind>) -> PyResult<()> {
        match params {
            Some(ParamKind::Positional(vals)) => {
                for (i, v) in vals.iter().enumerate() {
                    stmt.raw_bind_parameter(i + 1, v)
                        .map_err(|e| PyRuntimeError::new_err(e.to_string()))?;
                }
            }
            Some(ParamKind::Named(vals)) => {
                for (name, v) in vals {
                    let idx = stmt
                        .parameter_index(name)
                        .map_err(|e| PyRuntimeError::new_err(e.to_string()))?
                        .ok_or_else(|| {
                            PyValueError::new_err(format!("Unknown parameter: {}", name))
                        })?;
                    stmt.raw_bind_parameter(idx, v)
                        .map_err(|e| PyRuntimeError::new_err(e.to_string()))?;
                }
            }
            None => {}
        }

        Ok(())
    }

    fn execute_blocking(&self, sql: String, params: Option<ParamKind>) -> PyResult<Cursor> {
        let conn = self.get_connection()?;
        let _guard = ConnGuard {
            pool: self,
            conn: conn.clone(),
        };

        let db_guard = conn.db.lock().unwrap_or_else(|e| e.into_inner());
        let db = db_guard
            .as_ref()
            .ok_or_else(|| PyRuntimeError::new_err("Connection closed"))?;
        let mut stmt = db
            .prepare(&sql)
            .map_err(|e| PyRuntimeError::new_err(e.to_string()))?;

        Self::bind_params(&mut stmt, &params)?;

        let readonly = stmt.readonly();
        if readonly {
            let names: Vec<String> = stmt.column_names().iter().map(|s| s.to_string()).collect();
            let ncols = stmt.column_count();
            let mut out = Vec::new();
            {
                let mut rows_iter = stmt.raw_query();
                while let Some(row) = rows_iter
                    .next()
                    .map_err(|e| PyRuntimeError::new_err(e.to_string()))?
                {
                    let mut vals = Vec::with_capacity(ncols);
                    for i in 0..ncols {
                        let v: SqlValue = row.get(i).unwrap_or(SqlValue::Null);
                        vals.push(v);
                    }
                    out.push(vals);
                }
            }
            let _ = stmt.finalize();
            drop(db_guard);

            Ok(Cursor::new(out, names, -1))
        } else {
            let rc = stmt
                .raw_execute()
                .map_err(|e| PyRuntimeError::new_err(e.to_string()))?;
            let _ = stmt.finalize();
            drop(db_guard);

            Ok(Cursor::with_rowcount(rc as i64))
        }
    }

    fn executemany_blocking(&self, sql: String, rows: Vec<Option<ParamKind>>) -> PyResult<Cursor> {
        let conn = self.get_connection()?;
        let _guard = ConnGuard {
            pool: self,
            conn: conn.clone(),
        };

        if rows.is_empty() {
            return Ok(Cursor::empty());
        }

        let db_guard = conn.db.lock().unwrap_or_else(|e| e.into_inner());
        let db = db_guard
            .as_ref()
            .ok_or_else(|| PyRuntimeError::new_err("Connection closed"))?;

        let managed = !db.is_autocommit();
        if managed {
            db.execute_batch("BEGIN IMMEDIATE")
                .map_err(|e| PyRuntimeError::new_err(e.to_string()))?;
        }

        let mut stmt = match db.prepare(&sql) {
            Ok(s) => s,
            Err(e) => {
                if managed {
                    let _ = db.execute_batch("ROLLBACK");
                }
                return Err(PyRuntimeError::new_err(e.to_string()));
            }
        };

        let mut executed: i64 = 0;
        for row in &rows {
            stmt.clear_bindings();
            if let Err(e) = Self::bind_params(&mut stmt, row) {
                if managed {
                    let _ = db.execute_batch("ROLLBACK");
                }
                let _ = stmt.finalize();
                return Err(e);
            }

            if let Err(e) = stmt.raw_execute() {
                if managed {
                    let _ = db.execute_batch("ROLLBACK");
                }
                let _ = stmt.finalize();
                return Err(PyRuntimeError::new_err(e.to_string()));
            }
            executed += 1;
        }
        let _ = stmt.finalize();

        if managed && let Err(e) = db.execute_batch("COMMIT") {
            return Err(PyRuntimeError::new_err(e.to_string()));
        }

        drop(db_guard);
        Ok(Cursor::with_rowcount(executed))
    }
}

#[pymethods]
impl Pool {
    #[new]
    #[pyo3(signature = (path, pool_size = 4))]
    fn new(path: String, pool_size: i64) -> PyResult<Self> {
        if !(1..=256).contains(&pool_size) {
            return Err(PyValueError::new_err("pool_size must be between 1 and 256"));
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
        self.inner
            .lock()
            .unwrap_or_else(|e| e.into_inner())
            .pool_size
    }

    pub fn close(&self) {
        let mut inner = self.inner.lock().unwrap_or_else(|e| e.into_inner());
        inner.closed = true;
        for c in &inner.connections {
            c.close();
        }
    }

    #[pyo3(signature = (sql, params = None))]
    fn execute(&self, py: Python<'_>, sql: String, params: Option<Py<PyAny>>) -> PyResult<Cursor> {
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
        let mut inner = self.inner.lock().unwrap_or_else(|e| e.into_inner());
        inner.closed = true;
        for c in &inner.connections {
            c.close();
        }
    }
}
