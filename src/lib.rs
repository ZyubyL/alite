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
mod connection;
mod cursor;
mod pool;
mod row;
mod value;

use pyo3::prelude::*;

#[pymodule]
fn _alite(m: &Bound<'_, PyModule>) -> PyResult<()> {
    m.add_class::<pool::Pool>()?;
    m.add_class::<connection::Connection>()?;
    m.add_class::<cursor::Cursor>()?;
    m.add_class::<row::Row>()?;

    m.add("DEFAULT_POOL_SIZE", 4i64)?;
    m.add("MAX_POOL_SIZE", 256i64)?;

    Ok(())
}
