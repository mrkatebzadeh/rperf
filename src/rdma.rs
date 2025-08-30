/* rdma.rs

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
use rrddmma::prelude::{Mr, SendWr};
use rrddmma::rdma::mr::Permission;
use rrddmma::{ctrl, prelude::Slicing, rdma::qp::Qp};
use spdlog::{debug, info};
use std::alloc::{self, Layout};
use std::ptr;
use std::sync::Arc;

use crate::Config;

#[cfg(feature = "hugepage")]
const HUGEPAGE_SIZE: usize = 2 * 1024 * 1024;

#[derive(Clone)]
pub struct RdmaAdaptor {
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

unsafe impl Sync for RdmaAdaptor {}
unsafe impl Send for RdmaAdaptor {}

impl RdmaAdaptor {
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

    fn post_recv(&self, index: usize) {
        let start = index * self.config.test.msg_size;
        let mr_slice = unsafe { self.rx.slice_unchecked(start, self.config.test.msg_size) };
        self.qp
            .recv(&[mr_slice], index as u64)
            .expect("Network (RDMA): Failed to post recv");
    }

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
}

impl Drop for RdmaAdaptor {
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

        #[cfg(feature = "hugepage")]
        {
            debug!("Network (RDMA): Deallocating hugepages");
            let layout = Layout::array::<u8>(self.config.test.msg_size * self.config.test.tx_depth)
                .expect("Network (RDMA): Failed to create layout");
            unsafe { hugepage_rs::dealloc(self.tx_buf, layout) };

            let layout = Layout::array::<u8>(self.config.test.msg_size * self.config.test.rx_depth)
                .expect("Network (RDMA): Failed to create layout");
            unsafe { hugepage_rs::dealloc(self.rx_buf, layout) };
        }

        #[cfg(not(feature = "hugepage"))]
        {
            debug!("Network (RDMA): Deallocating buffers");
            let layout = Layout::array::<u8>(self.config.test.msg_size * self.config.test.tx_depth)
                .expect("Network (RDMA): Failed to create layout");
            unsafe { alloc::dealloc(self.tx_buf, layout) };

            let layout = Layout::array::<u8>(self.config.test.msg_size * self.config.test.rx_depth)
                .expect("Network (RDMA): Failed to create layout");
            unsafe { alloc::dealloc(self.rx_buf, layout) };
        }
    }
}
/* rdma.rs ends here */
