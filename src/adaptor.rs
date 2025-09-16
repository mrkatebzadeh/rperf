/* adaptor.rs

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

use crate::collector::SampleCollector;
use crate::message::Message;
use crate::Config;
use anyhow::{bail, Context};
use rand::Rng;
use rrddmma::prelude::{Cq, Nic, Pd, Qp, QpCaps, QpType};
use rrddmma::prelude::{Mr, SendWr};
use rrddmma::rdma::mr::Permission;
use rrddmma::{ctrl, prelude::Slicing};
use spdlog::{debug, info};
use std::alloc::{self, Layout};
use std::net::Ipv4Addr;
use std::ptr;
use std::str::FromStr;
use std::sync::Arc;

#[cfg(feature = "hugepage")]
const HUGEPAGE_SIZE: usize = 2 * 1024 * 1024;

/// Represents an RDMA adapter with configuration and queue pair information.
///
/// This struct manages the RDMA resources necessary for sending and receiving messages
/// over an RDMA network, including the configuration, queue pairs, and memory regions.
#[derive(Clone)]
pub struct Adaptor {
    pub(crate) config: Config,
    pub(crate) qp: Arc<Qp>,

    #[allow(unused)]
    pub(crate) tx_buf: *mut u8,
    #[allow(unused)]
    pub(crate) rx_buf: *mut u8,

    pub(crate) tx: Arc<Mr>,
    pub(crate) rx: Arc<Mr>,
    pub(crate) tx_collector: Arc<SampleCollector>,
    pub(crate) rx_collector: Arc<SampleCollector>,
}

unsafe impl Sync for Adaptor {}
unsafe impl Send for Adaptor {}

impl Adaptor {
    /// Polls the completion queue for a specified number of completions.
    ///
    /// This function will block until the specified number of completions have been
    /// polled from the send completion queue.
    fn poll_poll_cq(&self, num: usize) {
        let mut remaining = num;
        loop {
            let polled = match self.qp.scq().poll_some(remaining as u32) {
                Ok(wcs) => wcs.len(),
                Err(e) => panic!("Network (RDMA): Failed to poll Send CQ: {:?}", e),
            };
            remaining -= polled;
            if remaining == 0 {
                break;
            }
        }
    }

    /// Posts a receive request to the queue pair.
    ///
    /// This function prepares the memory region for receiving a message and posts
    /// a receive work request to the queue pair.
    fn post_recv(&self, index: usize) {
        let start = index * self.config.test.msg_size;
        let mr_slice = unsafe { self.rx.slice_unchecked(start, self.config.test.msg_size) };
        self.qp
            .recv(&[mr_slice], index as u64)
            .expect("Network (RDMA): Failed to post recv");
    }

    /// Allocates a buffer of the specified size.
    ///
    /// Depending on the feature flags, this function will attempt to allocate
    /// the buffer using hugepages if enabled, otherwise it falls back to standard
    /// memory allocation.
    pub(crate) fn buffer_allocate(size: usize) -> *mut u8 {
        if size == 0 {
            return ptr::null_mut();
        }

        let layout = Layout::array::<u8>(size).expect("Network (RDMA): Failed to create layout");
        #[cfg(feature = "hugepage")]
        {
            let ptr = unsafe { hugepage_rs::alloc(layout) as *mut u8 };

            if ptr.is_null() {
                panic!("Network (RDMA): Hugepage allocation failed");
            }

            return ptr;
        }

        let ptr = unsafe { alloc::alloc(layout) };

        if ptr.is_null() {
            panic!("Network (RDMA): Standard buffer allocation failed");
        }

        ptr
    }

    /// Writes a batch of messages to the RDMA network.
    ///
    /// This function sends a series of messages over RDMA, managing the necessary
    /// work requests and handling the completion notifications.
    fn write(&self, batch: &[Message]) {
        let mut rng = rand::rng();
        let sample_id: u64 = rng.random();

        let msg_size = self.config.test.msg_size;
        self.tx_collector.sample_start(sample_id);

        let mut wrs = Vec::with_capacity(batch.len());
        let mut prev_index: Option<usize> = None;

        for (index, msg) in batch.iter().enumerate() {
            let start = index * msg_size;
            let mr_slice = unsafe { self.tx.slice_unchecked(start, msg_size) };
            let mut wr = SendWr::<1>::default();
            wr.set_sge(0, &mr_slice)
                .set_flag_signaled()
                .set_id(msg.req_id())
                .set_wr_send(None)
                .set_sgl_len(1);

            wrs.push(wr);
            let curr_index = wrs.len() - 1;

            if let Some(prev) = prev_index {
                let (left, right) = wrs.split_at_mut(curr_index);
                left[prev].set_next(&right[0]);
            }

            prev_index = Some(curr_index);
        }

        unsafe {
            std::ptr::copy_nonoverlapping(
                batch.as_ptr() as *const u8,
                self.tx.as_slice().addr(),
                msg_size * batch.len(),
            );
        }

        wrs[0]
            .post_on(&self.qp)
            .expect("Network (RDMA) post a Send request");
        self.poll_poll_cq(batch.len());

        self.tx_collector.sample_end(sample_id);
    }

    /// Reads a batch of messages from the RDMA network.
    ///
    /// This function polls the receive completion queue and retrieves a batch of
    /// received messages, handling any necessary parsing and buffer management.
    fn read(&self) -> Vec<Message> {
        let mut msgs = vec![];
        let mut rng = rand::rng();
        let sample_id: u64 = rng.random();
        let msg_size = self.config.test.msg_size;
        let rx_depth = self.config.test.rx_depth;

        self.rx_collector.sample_start(sample_id);

        let wcs = match self.qp.rcq().poll_some(rx_depth as u32) {
            Ok(wcs) => wcs,
            Err(e) => panic!("Network (RDMA): Failed to read: {:?}", e),
        };

        let mut counter = 0;
        for wc in wcs {
            self.rx_collector.sample_end(sample_id);
            let _recv_size = match wc.ok() {
                Ok(n) => {
                    assert_eq!(n, msg_size);
                    n
                }
                Err(e) => {
                    panic!("Network (RDMA): failed to read {:?}", e);
                }
            };

            let index = wc.wr_id() as usize;

            let start = index * msg_size;
            let end = (index + 1) * msg_size;
            let buffer = &unsafe { self.rx.mem() }[start..end];

            let message = match Message::try_from(buffer) {
                Ok(message) => message,
                Err(e) => panic!("Network (RDMA): Failed to parse {:?}", e),
            };

            self.post_recv(index);
            msgs[counter] = message;
            counter += 1;
        }
        msgs
    }

    /// Establishes a connection using the specified configuration.
    ///
    /// This function sets up the RDMA resources based on the provided configuration
    /// and establishes a connection to the remote server using the RDMA network.
    pub fn connect(config: Config) -> anyhow::Result<Self>
    where
        Self: Sized,
    {
        let mut qp = Self::make_qp(&config)?;
        let ip = config.connection.server_addr.as_str();
        let port = config.connection.server_port;
        let msg_size = config.test.msg_size;
        let tx_depth = config.test.tx_depth;
        let rx_depth = config.test.rx_depth;

        let ip4addr = Ipv4Addr::from_str(ip)?;
        ctrl::Connecter::new_on_port(Some(ip4addr), port)
            .context("connector new on port")?
            .connect(&mut qp)
            .context("connector connect qps")?;
        info!("Network (RDMA) Connector is initialized {} : {}", ip, port);

        let qp = Arc::new(qp);

        let tx_buf = Adaptor::buffer_allocate(msg_size * tx_depth);
        let rx_buf = Adaptor::buffer_allocate(msg_size * rx_depth);

        let send_mr = unsafe {
            Mr::reg(qp.pd(), tx_buf, msg_size * tx_depth, Permission::default())
                .expect("Network (RDMA): Failed to register memory for send")
        };

        let recv_mr = unsafe {
            Mr::reg(qp.pd(), rx_buf, msg_size * rx_depth, Permission::default())
                .expect("Network (RDMA): Failed to register memory for recv")
        };
        let tx = Arc::new(send_mr);
        let rx = Arc::new(recv_mr);

        let rx_collector = Arc::new(SampleCollector::new(0));
        let tx_collector = Arc::new(SampleCollector::new(0));

        let adaptor = Self {
            config,
            qp,
            tx_buf,
            rx_buf,
            tx,
            rx,

            rx_collector,
            tx_collector,
        };

        match adaptor.setup() {
            Ok(_) => Ok(adaptor),
            Err(e) => bail!("{}", e),
        }
    }

    /// Prepares the RDMA adaptor for operation.
    ///
    /// This function posts initial receive requests to the queue pair and ensures
    /// the RDMA adapter is ready to send and receive messages.
    pub fn setup(&self) -> anyhow::Result<()> {
        for index in 0..self.config.test.rx_depth {
            self.post_recv(index);
        }

        info!("Network (RDMA) is ready.");
        Ok(())
    }

    /// Creates and configures a queue pair for RDMA operations.
    ///
    /// This function initializes the RDMA device, allocates the necessary resources,
    /// and sets up the queue pair for sending and receiving messages.
    pub(crate) fn make_qp(config: &Config) -> anyhow::Result<Qp> {
        let dev = config.device.name.as_str();
        let tx_depth = config.test.tx_depth;
        let rx_depth = config.test.rx_depth;
        let ib_port = config.device.ib_port;

        let Nic { context, ports } = Nic::finder()
            .dev_name(dev)
            .probe()
            .context("probe rdma device")?;
        info!("Network (RDMA) Device {} recognized", dev);

        let pd = Pd::new(&context).context("pd allocated")?;
        info!("Network (RDMA) PD allocated");

        let send_cq = Cq::new(&context, (tx_depth + 1) as u32).context("send_ccq allocated")?;
        info!("Network (RDMA) Send CQ allocated");

        let recv_cq = Cq::new(&context, (rx_depth + 1) as u32).context("recv_cq allocated")?;
        info!("Network (RDMA) Recv CQ allocated");

        let mut qp = Qp::builder()
            .qp_type(QpType::Rc)
            .caps(QpCaps::default())
            .send_cq(&send_cq)
            .recv_cq(&recv_cq)
            .sq_sig_all(true)
            .build(&pd)
            .context("qp built")?;
        info!("Network (RDMA) QP created");

        match qp.bind_local_port(&ports[ib_port as usize], None) {
            Ok(_) => {
                info!("Network (RDMA) QP bound to local port {}", ib_port);
                Ok(qp)
            }
            Err(e) => bail!("Network (RDMA): qp bound to local port {:?}", e),
        }
    }
}

impl Drop for Adaptor {
    /// Cleans up RDMA resources upon dropping the adaptors.
    ///
    /// This function deallocates any memory buffers and logs performance metrics
    /// before the RDMA adaptor is dropped.
    fn drop(&mut self) {
        let duration = self.tx_collector.duration();
        let mean_latency = self.tx_collector.mean_latency();
        info!(
            "Average [Send] latency reported by collector: {:?} in {:?}",
            mean_latency, duration
        );

        let duration = self.rx_collector.duration();
        let mean_latency = self.rx_collector.mean_latency();
        info!(
            "Average [Recv] latency reported by collector: {:?} in {:?}",
            mean_latency, duration
        );

        let msg_size = self.config.test.msg_size;
        let tx_depth = self.config.test.tx_depth;
        let rx_depth = self.config.test.rx_depth;

        #[cfg(feature = "hugepage")]
        {
            debug!("Network (RDMA): Deallocating hugepages");
            let layout = Layout::array::<u8>(msg_size * tx_depth)
                .expect("Network (RDMA): Failed to create layout");
            unsafe { hugepage_rs::dealloc(self.tx_buf, layout) };

            let layout = Layout::array::<u8>(msg_size * rx_depth)
                .expect("Network (RDMA): Failed to create layout");
            unsafe { hugepage_rs::dealloc(self.rx_buf, layout) };
        }

        #[cfg(not(feature = "hugepage"))]
        {
            debug!("Network (RDMA): Deallocating buffers");
            let layout = Layout::array::<u8>(msg_size * tx_depth)
                .expect("Network (RDMA): Failed to create layout");
            unsafe { alloc::dealloc(self.tx_buf, layout) };

            let layout = Layout::array::<u8>(msg_size * rx_depth)
                .expect("Network (RDMA): Failed to create layout");
            unsafe { alloc::dealloc(self.rx_buf, layout) };
        }
    }
}
/* adaptor.rs ends here */
