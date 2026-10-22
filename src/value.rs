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
use pyo3::exceptions::PyTypeError;
use pyo3::prelude::*;
use pyo3::types::{PyBytes, PyDict, PyString};
use rusqlite::types::Value as SqlValue;

pub fn py_to_sql(_py: Python<'_>, obj: &Bound<'_, PyAny>) -> PyResult<SqlValue> {
    if obj.is_none() {
        return Ok(SqlValue::Null);
    }

    if let Ok(v) = obj.extract::<bool>() {
        return Ok(SqlValue::Integer(v as i64));
    }

    if let Ok(v) = obj.extract::<i64>() {
        return Ok(SqlValue::Integer(v));
    }

    if let Ok(v) = obj.extract::<f64>() {
        return Ok(SqlValue::Real(v));
    }

    if let Ok(s) = obj.cast::<PyString>() {
        return Ok(SqlValue::Text(s.to_str()?.to_owned()));
    }

    if let Ok(b) = obj.cast::<PyBytes>() {
        return Ok(SqlValue::Blob(b.as_bytes().to_vec()));
    }

    Err(PyTypeError::new_err("Unsupported parameter type"))
}

pub fn sql_to_py(py: Python<'_>, v: &SqlValue) -> PyResult<Py<PyAny>> {
    match v {
        SqlValue::Null => Ok(py.None()),
        SqlValue::Integer(i) => Ok(i.into_pyobject(py)?.into_any().unbind()),
        SqlValue::Real(f) => Ok(f.into_pyobject(py)?.into_any().unbind()),
        SqlValue::Text(s) => Ok(s.into_pyobject(py)?.into_any().unbind()),
        SqlValue::Blob(b) => {
            let bytes = PyBytes::new(py, b);
            Ok(bytes.into_pyobject(py)?.into_any().unbind())
        }
    }
}

pub fn positional_params(
    py: Python<'_>,
    params: Option<&Bound<'_, PyAny>>,
) -> PyResult<Option<Vec<SqlValue>>> {
    let Some(p) = params else {
        return Ok(None);
    };

    if p.is_none() {
        return Ok(None);
    }

    let mut out = Vec::new();

    for item in p.try_iter()? {
        out.push(py_to_sql(py, &item?)?);
    }

    Ok(Some(out))
}

pub fn named_params(
    py: Python<'_>,
    params: &Bound<'_, PyAny>,
) -> PyResult<Vec<(String, SqlValue)>> {
    let dict = params.cast::<PyDict>()?;
    let mut out = Vec::with_capacity(dict.len());

    for (k, v) in dict.iter() {
        let key = k.extract::<String>()?;
        out.push((key, py_to_sql(py, &v)?));
    }

    Ok(out)
}
