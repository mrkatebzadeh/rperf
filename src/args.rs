/* args.rs

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

use clap::Parser;
use std::fmt::{self, Display};

#[derive(Parser)]
#[command(version, about, long_about = None, term_width = 80)]
pub struct Args {
    #[arg(short = 'd', long, value_name = "device", help = "RDMA device name")]
    #[clap(default_value = "mlx5_0")]
    pub device: String,

    #[arg(short = 'p', long, help = "RDMA Port")]
    #[clap(default_value_t = 0)]
    pub ib_port: usize,

    #[arg(short = 'c', long, value_name = "FILE", help = "Configuration file")]
    #[clap(default_value = "config.toml")]
    pub config: String,

    #[arg(short, long, help = "Number of total operations")]
    #[clap(default_value_t = 100000)]
    pub operations: u32,

    #[arg(short, long, action = clap::ArgAction::Count)]
    #[arg(help = "Increment verbosity level (repeat for more detail, e.g., -vvv)")]
    pub verbose: u8,
}

impl Display for Args {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "Args:\n\
            Device:     {}\n\
            IB Port:    {}\n\
            Config:     {}\n\
            Operations: {}\n\
            Verbose:    {}",
            self.device, self.ib_port, self.config, self.operations, self.verbose
        )
    }
}
/* args.rs ends here */
