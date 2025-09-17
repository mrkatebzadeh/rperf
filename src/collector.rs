/* collector.rs

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
    collections::BTreeMap,
    fs::File,
    io::{BufWriter, Write},
    sync::{
        atomic::{AtomicU64, Ordering},
        Arc, Mutex,
    },
    time::{Duration, Instant},
};

#[inline]
pub fn rdtsc() -> u64 {
    #[cfg(target_arch = "x86_64")]
    unsafe {
        let hi: u32;
        let lo: u32;
        core::arch::asm!(
            "rdtsc",
            out("eax") lo,
            out("edx") hi,
            options(nomem, nostack, preserves_flags)
        );
        ((hi as u64) << 32) | (lo as u64)
    }

    #[cfg(target_arch = "aarch64")]
    unsafe {
        let cntvct: u64;
        core::arch::asm!(
            "mrs {0}, cntvct_el0",
            out(reg) cntvct,
            options(nomem, nostack, preserves_flags)
        );
        cntvct
    }

    #[cfg(target_arch = "arm")]
    unsafe {
        let pmccntr: u32;
        core::arch::asm!(
            "mrc p15, 0, {0}, c9, c13, 0",
            out(reg) pmccntr,
            options(nomem, nostack, preserves_flags)
        );
        pmccntr as u64
    }

    #[cfg(not(any(target_arch = "x86_64", target_arch = "aarch64", target_arch = "arm")))]
    compile_error!("rdtsc() not supported on this architecture");
}

#[inline]
fn counter_freq() -> u64 {
    calcmhz::mhz().unwrap_or(1000.0) as u64 * 1_000_000
}

fn cycles_to_nanos(cycles: u64, freq: u64) -> u64 {
    cycles.saturating_mul(1_000_000_000) / freq
}

#[allow(unused)]
#[derive(Clone)]
pub struct SampleCollector {
    samples: Arc<Mutex<Vec<u64>>>,
    start: Arc<Mutex<Option<Instant>>>,
    end: Arc<Mutex<Option<Instant>>>,
    size: Arc<AtomicU64>,
    filename: Arc<Mutex<String>>,
}

impl SampleCollector {
    #[allow(unused)]
    pub fn new(size: u64, filename: impl AsRef<str>) -> Self {
        Self {
            samples: Arc::new(Mutex::new(vec![])),
            start: Arc::new(Mutex::new(None)),
            end: Arc::new(Mutex::new(None)),
            size: Arc::new(AtomicU64::new(size)),
            filename: Arc::new(Mutex::new(filename.as_ref().to_owned())),
        }
    }

    #[allow(unused)]
    pub fn record_start(&self) {
        let mut lock = self
            .start
            .lock()
            .expect("Sample: Failed to get the lock for start");

        if lock.is_none() {
            *lock = Some(Instant::now());
        }
    }

    #[allow(unused)]
    pub fn record_end(&self) {
        let mut lock = self
            .end
            .lock()
            .expect("Sample: Failed to get the lock for end");

        *lock = Some(Instant::now());
    }

    #[allow(unused)]
    #[inline(always)]
    pub fn sample(&self) -> u64 {
        rdtsc()
    }

    pub fn insert(&self, sample: u64) {
        let mut samples_guard = self.samples.lock().unwrap();
        samples_guard.push(sample);
    }

    #[allow(unused)]
    pub fn print_latency_histogram(&self) {
        let freq = counter_freq();
        let latencies = self.samples.lock().unwrap();
        let mut histogram: BTreeMap<u64, u32> = BTreeMap::new();

        for entry in latencies.iter() {
            *histogram.entry(*entry).or_insert(0) += 1;
        }

        println!("Latency Histogram (ns):");
        for (latency, count) in histogram {
            println!("{:<10} ns | {}", latency, "*".repeat(count as usize));
        }
    }

    #[allow(unused)]
    pub fn mean_latency(&self) -> Option<std::time::Duration> {
        let freq = counter_freq();
        let mut total_cycles = 0u128;
        let mut count = 0u128;

        let latencies = self.samples.lock().unwrap();
        for entry in latencies.iter() {
            total_cycles += *entry as u128;
        }

        if count > 0 {
            let nanos = total_cycles.saturating_mul(1_000_000_000) / freq as u128 / count;
            Some(std::time::Duration::from_nanos(nanos as u64))
        } else {
            None
        }
    }

    #[allow(unused)]
    pub fn quantile_latency(&self, quantile: f64) -> Option<std::time::Duration> {
        let freq = counter_freq();
        let mut latencies = self.samples.lock().unwrap().clone();

        if latencies.is_empty() {
            return None;
        }

        latencies.sort_unstable();
        let index = (latencies.len() as f64 * quantile).ceil() as usize - 1;
        let cycles = latencies[index.min(latencies.len() - 1)];
        let nanos = cycles_to_nanos(cycles, freq);
        Some(std::time::Duration::from_nanos(nanos))
    }

    #[allow(unused)]
    pub fn throughput(&self) -> Option<f64> {
        let ops = self.size.load(Ordering::Acquire);
        if let Some(duration) = self.duration() {
            let duration = duration.as_secs_f64();
            if duration > 0.0 {
                return Some((ops as f64 / duration) / 1_000_000.0);
            }
        }

        None
    }

    #[allow(unused)]
    pub fn duration(&self) -> Option<Duration> {
        let start = self.start.lock().expect("Sample: Failed to lock for start");
        let end = self.end.lock().expect("Sample: Failed to lock for end");
        let count = self.size.load(Ordering::Acquire);

        if let (Some(start), Some(end)) = (*start, *end) {
            Some(end.duration_since(start))
        } else {
            None
        }
    }
    pub fn dump_csv(&self) -> anyhow::Result<()> {
        let filename = self.filename.lock().unwrap();
        let file = File::create(filename.as_str())?;
        let mut writer = BufWriter::new(file);

        writeln!(writer, "cycles_diff")?;

        let latencies = self.samples.lock().unwrap();
        for entry in latencies.iter() {
            writeln!(writer, "{}", *entry)?;
        }

        writer.flush()?;
        Ok(())
    }
}
/* collector.rs ends here */
