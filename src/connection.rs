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
use rusqlite::Connection as SqliteConn;
use std::sync::{Arc, Mutex};

#[pyclass(name = "Connection")]
#[derive(Clone)]
pub struct Connection {
    pub db: Arc<Mutex<Option<SqliteConn>>>,
    pub in_use: Arc<Mutex<bool>>,
}

impl Connection {
    pub fn open_db(path: &str) -> PyResult<Self> {
        let conn = SqliteConn::open(path)
            .map_err(|e| pyo3::exceptions::PyConnectionError::new_err(e.to_string()))?;
        conn.execute_batch("PRAGMA journal_mode=WAL; PRAGMA busy_timeout = 5000;")
            .map_err(|e| pyo3::exceptions::PyRuntimeError::new_err(e.to_string()))?;
        Ok(Self {
            db: Arc::new(Mutex::new(Some(conn))),
            in_use: Arc::new(Mutex::new(false)),
        })
    }
}

#[pymethods]
impl Connection {
    #[new]
    fn new() -> Self {
        Self {
            db: Arc::new(Mutex::new(None)),
            in_use: Arc::new(Mutex::new(false)),
        }
    }

    pub fn close(&self) {
        let mut guard = self.db.lock().unwrap();
        if let Some(c) = guard.take() {
            let _ = c.close();
        }
        *self.in_use.lock().unwrap() = false;
    }
}

impl Drop for Connection {
    fn drop(&mut self) {
        // Connection is a shared handle (Arc). Do not close the DB here;
        // closing is managed by Pool (see Drop for Pool).
    }
}
