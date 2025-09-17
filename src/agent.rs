/* agent.rs

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

use crate::{server::Server, Config};
use spdlog::info;
use std::thread;

pub(crate) struct Agent {
    config: Config,
}
impl Agent {
    pub(crate) fn new(config: Config) -> Self {
        Self { config }
    }

    pub(crate) fn start(&mut self) -> anyhow::Result<()> {
        info!("Agent: server binding.");
        let mut server = Server::bind(self.config.clone())?;
        loop {
            let adaptor = server.accept()?;

            thread::spawn(move || loop {
                let _ = adaptor.read();
            });
        }

        Ok(())
    }
}

/* agent.rs ends here */
