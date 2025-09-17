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

use std::{
    sync::{
        atomic::{AtomicBool, Ordering},
        Arc,
    },
    thread,
};

use spdlog::info;

use crate::{adaptor::Adaptor, message::Message, server::Server, Config};

pub(crate) struct Probe {
    config: Config,
}
impl Probe {
    pub(crate) fn new(config: Config) -> Self {
        Self { config }
    }

    pub(crate) fn start(&mut self) -> anyhow::Result<()> {
        let mut config = self.config.clone();
        let running = Arc::new(AtomicBool::new(true));
        let r = running.clone();
        let srv_handler = thread::spawn(move || {
            info!("Starting probe loopback server");
            config.is_agent = true;
            config.connection.server_addr = "0.0.0.0".to_string();
            config.connection.server_port -= 1;
            let loopback_srv = Server::bind(config).unwrap();
            info!("Probe loopback server connected");
            let loopback_adaptor = loopback_srv.accept().unwrap();
            while r.load(Ordering::Relaxed) {
                let _ = loopback_adaptor.read();
            }
        });

        info!("Connecting to agent");
        let mut wire_adaptor = Adaptor::connect(self.config.clone())?;
        info!("Connecting to loopback server");
        let mut loopback_adaptor = {
            let mut config = self.config.clone();
            config.connection.server_addr = "0.0.0.0".to_string();
            Adaptor::connect(self.config.clone())?
        };
        let total_iters = self.config.test.iterations;
        let warmup_iters = (total_iters as f64 * 0.1).ceil() as usize;
        let msg_size = self.config.test.msg_size;
        info!("Warming up");
        for id in 0..warmup_iters {
            let id = id as u64;
            let _ = loopback_adaptor.get_rtt(&[Message::new(msg_size, id)]);
            let _ = wire_adaptor.get_rtt(&[Message::new(msg_size, id)]);
        }
        info!("Test started");
        for id in 0..total_iters {
            let id = id as u64;
            let loop_rtt = loopback_adaptor.get_rtt(&[Message::new(msg_size, id)]);
            let wire_rtt = wire_adaptor.get_rtt(&[Message::new(msg_size, id)]);

            wire_adaptor.tx_collector.insert((wire_rtt, loop_rtt));
        }

        wire_adaptor
            .tx_collector
            .dump_csv()
            .expect("failed to dump");
        info!("Test finished");
        running.store(false, Ordering::Relaxed);
        std::mem::drop(srv_handler);
        Ok(())
    }
}

/* probe.rs ends here */
