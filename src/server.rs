/* server.rs

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

use crate::adaptor::Adaptor;
use crate::collector::SampleCollector;
use crate::Config;
use anyhow::{bail, Context};
use rrddmma::ctrl::Connecter;
use rrddmma::prelude::Mr;
use rrddmma::rdma::mr::Permission;
use spdlog::{info, trace};
use std::sync::Arc;

pub struct Server {
    config: Config,
}

impl Server {
    pub fn bind(config: Config) -> anyhow::Result<Self> {
        Ok(Self { config })
    }

    pub fn accept(&self) -> anyhow::Result<Adaptor> {
        let ip = self.config.connection.server_addr.as_str();
        let port = self.config.connection.server_port;
        trace!("Network (RDMA) Listening {} : {}", ip, port);

        let listener = Connecter::new_on_port(None, port).context("connector new on port")?;
        info!("Network (RDMA) Connector is initialized {} : {}", ip, port);

        let mut qp = Adaptor::make_qp(&self.config).context("RDMA Server: make qp§")?;

        listener
            .connect(&mut qp)
            .context("RDMA Server: connect qps")?;
        let qp = Arc::new(qp);

        let tx_buf =
            Adaptor::buffer_allocate(self.config.test.msg_size * self.config.test.tx_depth);
        let rx_buf =
            Adaptor::buffer_allocate(self.config.test.msg_size * self.config.test.rx_depth);

        let send_mr = unsafe {
            Mr::reg(
                qp.pd(),
                tx_buf,
                self.config.test.msg_size * self.config.test.tx_depth,
                Permission::default(),
            )
            .expect("Network (RDMA): Failed to register memory for send")
        };

        let recv_mr = unsafe {
            Mr::reg(
                qp.pd(),
                rx_buf,
                self.config.test.msg_size * self.config.test.rx_depth,
                Permission::default(),
            )
            .expect("Network (RDMA): Failed to register memory for recv")
        };
        let tx = Arc::new(send_mr);
        let rx = Arc::new(recv_mr);

        let rx_collector = Arc::new(SampleCollector::new(0));

        let tx_collector = Arc::new(SampleCollector::new(0));

        let adaptor = Adaptor {
            config: self.config.clone(),
            qp,
            tx_buf,
            rx_buf,
            tx,
            rx,
            tx_collector,
            rx_collector,
        };

        match adaptor.setup() {
            Ok(_) => Ok(adaptor),
            Err(e) => bail!("{}", e),
        }
    }
}

/* server.rs ends here */
