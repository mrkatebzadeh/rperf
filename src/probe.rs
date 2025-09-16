/* probe.rs

*
* Author: M.R.Siavash Katebzadeh <mr@katebzadeh.xyz>
* Keywords: Rust
* Version: 0.0.1
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

use crate::{adaptor::Adaptor, Config};

pub(crate) struct Probe {
    config: Config,
}
impl Probe {
    pub(crate) fn new(config: Config) -> Self {
        Self { config }
    }

    pub(crate) fn start(&mut self) -> anyhow::Result<()> {
        let mut adaptor = Adaptor::connect(self.config.clone())?;
        Ok(())
    }
}

/* probe.rs ends here */
