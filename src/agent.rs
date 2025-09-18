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

/// Represents an agent responsible for managing network configurations and connections.
///
/// The `Agent` struct interacts with the network `Server`, handling network adaptor
/// acceptance and configuration.
pub(crate) struct Agent {
    config: Config,
}
impl Agent {
    /// Creates a new `Agent` instance with the given configuration.
    ///
    /// # Arguments
    ///
    /// * `config` - A `Config` struct that specifies the configuration for the agent.
    pub(crate) fn new(config: Config) -> Self {
        Self { config }
    }

    /// Starts the agent, binding to the server and accepting network adaptors.
    ///
    /// This function runs in a loop, accepting connections and spawning a new thread
    /// for each network adaptor to handle its operations.
    ///
    /// # Returns
    ///
    /// Returns `Ok(())` on success, or an error if the server fails to bind or accept an adaptor.
    pub(crate) fn start(&mut self) -> anyhow::Result<()> {
        info!("Agent: server binding.");
        let server = Server::bind(self.config.clone())?;
        loop {
            let adaptor = server.accept()?;

            thread::spawn(move || loop {
                let _ = adaptor.read();
            });
        }
    }
}

/* agent.rs ends here */
