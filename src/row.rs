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
use pyo3::exceptions::{PyIndexError, PyTypeError};
use pyo3::prelude::*;
use pyo3::types::{PyBool, PyDict, PyIterator, PyList, PyTuple};

#[pyclass(name = "Row")]
pub struct Row {
    values: Py<PyTuple>,
    names: Py<PyTuple>,
    index: Py<PyDict>,
}

impl Row {
    pub fn new(
        _py: Python<'_>,
        values: Py<PyTuple>,
        names: Py<PyTuple>,
        index: Py<PyDict>,
    ) -> PyResult<Self> {
        Ok(Self {
            values,
            names,
            index,
        })
    }
}

#[pymethods]
impl Row {
    fn __getitem__(&self, py: Python<'_>, key: &Bound<'_, PyAny>) -> PyResult<Py<PyAny>> {
        let idx: usize = if let Ok(i) = key.extract::<isize>() {
            if i < 0 {
                let n = self.values.bind(py).len() as isize;
                let i = (i % n + n) % n;

                i as usize
            } else {
                i as usize
            }
        } else if let Ok(name) = key.extract::<String>() {
            let index = self.index.bind(py);
            let idx_obj = index
                .get_item(&name)?
                .ok_or_else(|| PyIndexError::new_err("No item with that key"))?;
            idx_obj.extract::<usize>()?
        } else {
            return Err(PyTypeError::new_err("Row indices must be int or str"));
        };

        let values = self.values.bind(py);
        if idx >= values.len() {
            return Err(PyIndexError::new_err("index out of range"));
        }

        let v = values.get_item(idx)?;

        Ok(v.unbind())
    }

    fn __len__(&self, py: Python<'_>) -> usize {
        self.values.bind(py).len()
    }

    fn __iter__(&self, py: Python<'_>) -> PyResult<Py<PyIterator>> {
        let obj = self.values.bind(py).clone().into_any();
        Ok(PyIterator::from_object(&obj)?.unbind())
    }

    fn __repr__(&self, py: Python<'_>) -> PyResult<String> {
        Ok(format!("<alite.Row {}>", self.values.bind(py)))
    }

    fn keys(&self, py: Python<'_>) -> PyResult<Py<PyList>> {
        let names = self.names.bind(py);
        let list = PyList::new(py, names.iter())?;

        Ok(list.unbind())
    }

    #[pyo3(name = "__eq__")]
    fn eq(&self, py: Python<'_>, other: &Bound<'_, PyAny>) -> PyResult<Py<PyAny>> {
        let other_values = if let Ok(row) = other.cast::<Row>() {
            row.borrow().values.bind(py).clone()
        } else if let Ok(t) = other.cast::<PyTuple>() {
            t.clone()
        } else {
            return Ok(py.NotImplemented());
        };
        let res = self.values.bind(py).eq(&other_values)?;
        let b = PyBool::new(py, res);

        Ok((*b).clone().into_any().unbind())
    }

    #[pyo3(name = "__ne__")]
    fn ne(&self, py: Python<'_>, other: &Bound<'_, PyAny>) -> PyResult<Py<PyAny>> {
        let other_values = if let Ok(row) = other.cast::<Row>() {
            row.borrow().values.bind(py).clone()
        } else if let Ok(t) = other.cast::<PyTuple>() {
            t.clone()
        } else {
            return Ok(py.NotImplemented());
        };
        let res = self.values.bind(py).ne(&other_values)?;
        let b = PyBool::new(py, res);

        Ok((*b).clone().into_any().unbind())
    }
}
