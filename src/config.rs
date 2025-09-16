/* config.rs

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

use serde::Deserialize;

#[derive(Deserialize, Debug, Default, Clone)]
pub(crate) enum TestType {
    #[default]
    AckRTT,
}

#[derive(Deserialize, Debug, Clone)]
#[serde(default)]
pub(crate) struct Device {
    pub(crate) name: String,
    pub(crate) ib_port: u16,
}
impl Default for Device {
    fn default() -> Self {
        Self {
            name: "mlx5_0".to_string(),
            ib_port: 0,
        }
    }
}

#[derive(Deserialize, Debug, Clone)]
#[serde(default)]
pub(crate) struct Connection {
    pub(crate) server_port: u16,
    pub(crate) server_addr: String,
}
impl Default for Connection {
    fn default() -> Self {
        Self {
            server_port: 9999,
            server_addr: "0.0.0.0".to_string(),
        }
    }
}

#[derive(Deserialize, Debug, Clone)]
#[serde(default)]
pub(crate) struct Test {
    pub(crate) test_type: TestType,
    pub(crate) msg_size: usize,
    pub(crate) tx_depth: usize,
    pub(crate) rx_depth: usize,
    pub(crate) qps_num: usize,
    pub(crate) concurrent_msgs: usize,
    pub(crate) iterations: usize,
    pub(crate) duration: usize,
    pub(crate) burst_size: usize,
    pub(crate) rate_limiter: usize,
}
impl Default for Test {
    fn default() -> Self {
        Self {
            test_type: TestType::AckRTT,
            msg_size: 64,
            tx_depth: 8000,
            rx_depth: 8000,
            qps_num: 1,
            concurrent_msgs: 1,
            iterations: 5_000_000,
            duration: 15,
            burst_size: 1,
            rate_limiter: 15_000,
        }
    }
}

#[derive(Deserialize, Debug, Clone)]
#[serde(default)]
pub(crate) struct RDMAThreads {
    pub(crate) post_send: usize,
    pub(crate) poll_recv: usize,
    pub(crate) poll_send: usize,
}
impl Default for RDMAThreads {
    fn default() -> Self {
        Self {
            post_send: 1,
            poll_recv: 1,
            poll_send: 1,
        }
    }
}

#[derive(Deserialize, Debug, Clone)]
#[serde(default)]
pub(crate) struct Output {
    pub(crate) filename: String,
    pub(crate) show_result: bool,
}
impl Default for Output {
    fn default() -> Self {
        Self {
            filename: "histogram".into(),
            show_result: true,
        }
    }
}

#[derive(Deserialize, Debug, Clone)]
#[serde(default)]
pub(crate) struct Sample {
    pub(crate) enabled: bool,
    pub(crate) ratio: f32,
}
impl Default for Sample {
    fn default() -> Self {
        Self {
            enabled: true,
            ratio: 0.01,
        }
    }
}

#[derive(Deserialize, Debug, Default, Clone)]
#[serde(default)]
pub(crate) struct BWControl {
    pub(crate) limiter: bool,
}

fn default_client_threads() -> RDMAThreads {
    RDMAThreads {
        post_send: 1,
        poll_recv: 1,
        poll_send: 1,
    }
}
fn default_server_threads() -> RDMAThreads {
    RDMAThreads {
        post_send: 1,
        poll_recv: 1,
        poll_send: 1,
    }
}

#[derive(Deserialize, Debug, Clone)]
#[serde(default)]
pub(crate) struct Config {
    pub(crate) device: Device,
    pub(crate) connection: Connection,
    pub(crate) test: Test,

    #[serde(default = "default_client_threads")]
    pub(crate) client_threads: RDMAThreads,
    #[serde(default = "default_server_threads")]
    pub(crate) server_threads: RDMAThreads,

    pub(crate) output: Output,
    pub(crate) sample: Sample,
    pub(crate) bw_control: BWControl,
    pub(crate) is_agent: bool,
}
impl Default for Config {
    fn default() -> Self {
        Self {
            device: Device::default(),
            connection: Connection::default(),
            test: Test::default(),
            client_threads: default_client_threads(),
            server_threads: default_server_threads(),
            output: Output::default(),
            sample: Sample::default(),
            bw_control: BWControl::default(),
            is_agent: false,
        }
    }
}

/* config.rs ends here */
