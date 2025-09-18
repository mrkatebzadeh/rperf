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

/// Reads the current timestamp counter value.
///
/// Supports the following architectures:
/// - x86_64
/// - aarch64
/// - arm
///
/// # Returns
/// A `u64` value representing the current timestamp counter.
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

/// Returns the frequency of the counter in Hz.
///
/// Uses `calcmhz` crate to calculate the CPU frequency or defaults to 1000 MHz.
#[inline]
fn counter_freq() -> u64 {
    calcmhz::mhz().unwrap_or(1000.0) as u64 * 1_000_000
}

/// Converts cycles to nanoseconds based on the given frequency.
///
/// # Arguments
///
/// * `cycles` - The number of cycles.
/// * `freq` - The frequency of the counter in Hz.
///
/// # Returns
/// The equivalent duration in nanoseconds as a `u64`.
fn cycles_to_nanos(cycles: u64, freq: u64) -> u64 {
    cycles.saturating_mul(1_000_000_000) / freq
}

/// Represents a single latency sample with wire and loop round-trip times.
#[derive(Clone)]
pub struct Sample {
    pub wire_rtt: u64,
    pub loop_rtt: u64,
}

/// Collects and manages latency samples, providing statistical analysis.
#[allow(unused)]
#[derive(Clone)]
pub struct SampleCollector {
    samples: Vec<Sample>,
    start: Arc<Mutex<Option<Instant>>>,
    end: Arc<Mutex<Option<Instant>>>,
    size: Arc<AtomicU64>,
    filename: String,
}

impl SampleCollector {
    /// Creates a new `SampleCollector` with the specified sample size and filename.
    ///
    /// # Arguments
    ///
    /// * `size` - The maximum number of samples.
    /// * `filename` - The name of the file for storing samples.
    #[allow(unused)]
    pub fn new(size: u64, filename: impl AsRef<str>) -> Self {
        Self {
            samples: vec![],
            start: Arc::new(Mutex::new(None)),
            end: Arc::new(Mutex::new(None)),
            size: Arc::new(AtomicU64::new(size)),
            filename: filename.as_ref().to_owned(),
        }
    }

    /// Records the start time of the sampling.
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

    /// Records the end time of the sampling.
    #[allow(unused)]
    pub fn record_end(&self) {
        let mut lock = self
            .end
            .lock()
            .expect("Sample: Failed to get the lock for end");

        *lock = Some(Instant::now());
    }

    /// Samples the current timestamp counter value.
    #[allow(unused)]
    #[inline(always)]
    pub fn sample(&self) -> u64 {
        rdtsc()
    }

    /// Inserts a new latency sample into the collector.
    ///
    /// # Arguments
    ///
    /// * `sample` - A tuple containing wire RTT and loop RTT.
    pub fn insert(&mut self, sample: (u64, u64)) {
        self.samples.push(Sample {
            wire_rtt: sample.0,
            loop_rtt: sample.1,
        });
    }

    /// Prints a histogram of latency samples.
    ///
    /// Calculates the difference between wire and loop RTT for each sample
    /// and prints a histogram displaying the frequency of each latency value.
    #[allow(unused)]
    pub fn print_latency_histogram(&self) {
        let freq = counter_freq();
        let latencies = self.samples.clone();
        let mut histogram: BTreeMap<u64, u32> = BTreeMap::new();

        for entry in latencies.iter() {
            *histogram
                .entry(entry.wire_rtt - entry.loop_rtt)
                .or_insert(0) += 1;
        }

        println!("Latency Histogram (ns):");
        for (latency, count) in histogram {
            println!("{:<10} ns | {}", latency, "*".repeat(count as usize));
        }
    }

    /// Calculates the mean latency from samples.
    ///
    /// Computes the average difference between wire and loop RTT, converting
    /// cycles to nanoseconds based on the counter frequency.
    ///
    /// # Returns
    ///
    /// An `Option` containing the mean latency as `std::time::Duration`, or `None`
    /// if there are no samples.
    #[allow(unused)]
    pub fn mean_latency(&self) -> Option<std::time::Duration> {
        let freq = counter_freq();

        let mut latencies: Vec<u128> = self
            .samples
            .iter()
            .map(|s| s.wire_rtt.saturating_sub(s.loop_rtt) as u128)
            .collect();
        let total_cycles: u128 = latencies.iter().sum();

        let nanos =
            total_cycles.saturating_mul(1_000_000_000) / freq as u128 / latencies.len() as u128;
        Some(std::time::Duration::from_nanos(nanos as u64))
    }

    /// Calculates the quantile latency from samples.
    ///
    /// Computes the specified quantile of the difference between wire and loop RTT,
    /// converting cycles to nanoseconds based on the counter frequency.
    ///
    /// # Arguments
    ///
    /// * `quantile` - The quantile to compute (e.g., 0.5 for median).
    ///
    /// # Returns
    ///
    /// An `Option` containing the quantile latency as `std::time::Duration`, or `None`
    /// if there are no samples.
    #[allow(unused)]
    pub fn quantile_latency(&self, quantile: f64) -> Option<std::time::Duration> {
        let freq = counter_freq();
        let mut latencies: Vec<u64> = self
            .samples
            .iter()
            .map(|s| s.wire_rtt.saturating_sub(s.loop_rtt) as u64)
            .collect();

        if latencies.is_empty() {
            return None;
        }

        latencies.sort_unstable();
        let index = (latencies.len() as f64 * quantile).ceil() as usize - 1;
        let cycles = latencies[index.min(latencies.len() - 1)];
        let nanos = cycles_to_nanos(cycles, freq);
        Some(std::time::Duration::from_nanos(nanos))
    }

    /// Calculates the throughput based on the number of operations and duration.
    ///
    /// Computes the throughput as millions of operations per second.
    ///
    /// # Returns
    ///
    /// An `Option` containing the throughput in Mops, or `None` if duration
    /// cannot be determined.
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

    /// Calculates the total duration of the sampling period.
    ///
    /// # Returns
    ///
    /// An `Option` containing the duration as `Duration`, or `None` if
    /// start or end times are not recorded.
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
    /// Dumps the collected samples to a CSV file.
    ///
    /// Writes the wire RTT, loop RTT, and their difference to a CSV file
    /// specified by the `filename`.
    ///
    /// # Returns
    ///
    /// An `anyhow::Result<()>`, indicating success or failure of the file operation.
    pub fn dump_csv(&self) -> anyhow::Result<()> {
        let filename = &self.filename;
        let file = File::create(filename.as_str())?;
        let mut writer = BufWriter::new(file);

        writeln!(writer, "wire_rtt, loop_rtt, cycles_diff")?;

        for entry in self.samples.iter() {
            writeln!(
                writer,
                "{},{},{}",
                entry.wire_rtt,
                entry.loop_rtt,
                entry.wire_rtt.saturating_sub(entry.loop_rtt)
            )?;
        }

        writer.flush()?;
        Ok(())
    }
}
/* collector.rs ends here */
