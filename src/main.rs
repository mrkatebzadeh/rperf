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

mod args;
mod client;
mod config;
mod server;

use args::Args;
use clap::Parser;
use client::Client;
use config::Config;
use server::Server;
use spdlog::{self, info, Level, LevelFilter, Logger};
use std::fs;
use std::path::Path;
use std::sync::Arc;

fn main() -> anyhow::Result<()> {
    let args = Args::parse();

    let path = Path::new(&args.config);
    let raw = fs::read_to_string(path)?;

    let config: Config = toml::from_str(&raw)?;

    let level = match args.verbose {
        0 => Level::Warn,
        1 => Level::Info,
        2 => Level::Debug,
        _ => Level::Trace,
    };

    let default_logger: Arc<Logger> = spdlog::default_logger();
    default_logger.set_level_filter(LevelFilter::MoreSevereEqual(level));

    info!("{}", args);
    info!("Parsed config from {}:\n{:#?}", path.display(), config);

    if config.is_server {
        let mut server = Server::new(config);
        server.start()
    } else {
        let mut client = Client::new(config);
        client.start()
    }
}

/* main.rs ends here */
