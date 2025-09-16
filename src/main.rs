/* main.rs

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

mod adaptor;
mod agent;
mod args;
mod collector;
mod config;
mod message;
mod probe;
mod server;

use agent::Agent;
use args::Args;
use clap::Parser;
use config::Config;
use probe::Probe;
use spdlog::{self, info, Level, LevelFilter, Logger};
use std::fs;
use std::path::Path;
use std::sync::Arc;

fn main() -> anyhow::Result<()> {
    let args = Args::parse();

    let config = match fs::read_to_string(Path::new(&args.config)) {
        Ok(raw) => toml::from_str(&raw)?,
        Err(_) => Config::default(),
    };

    let level = match args.verbose {
        0 => Level::Warn,
        1 => Level::Info,
        2 => Level::Debug,
        _ => Level::Trace,
    };

    let default_logger: Arc<Logger> = spdlog::default_logger();
    default_logger.set_level_filter(LevelFilter::MoreSevereEqual(level));

    info!("Args: {}", &args);
    info!("Config: {:?}", &config);

    if config.is_agent {
        let mut agent = Agent::new(config);
        agent.start()
    } else {
        let mut probe = Probe::new(config);
        probe.start()
    }
}

/* main.rs ends here */
