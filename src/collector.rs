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
    sync::{
        atomic::{AtomicU64, Ordering},
        Arc, Mutex,
    },
    time::{Duration, Instant},
};

use dashmap::DashMap;

#[allow(unused)]
#[derive(Clone)]
pub struct SampleCollector {
    samples: Arc<DashMap<u64, (Instant, Option<Instant>)>>,
    start: Arc<Mutex<Option<Instant>>>,
    end: Arc<Mutex<Option<Instant>>>,
    size: Arc<AtomicU64>,
}

impl SampleCollector {
    #[allow(unused)]
    pub fn new(size: u64) -> Self {
        Self {
            samples: Arc::new(DashMap::new()),
            start: Arc::new(Mutex::new(None)),
            end: Arc::new(Mutex::new(None)),
            size: Arc::new(AtomicU64::new(size)),
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
    pub fn sample_start(&self, key: u64) {
        self.samples.insert(key, (Instant::now(), None));
    }

    #[allow(unused)]
    pub fn sample_end(&self, key: u64) {
        if let Some(mut entry) = self.samples.get_mut(&key) {
            entry.1 = Some(Instant::now());
        }
    }

    #[allow(unused)]
    pub fn get_sample(&self, key: u64) -> Option<(Instant, Option<Instant>)> {
        self.samples.get(&key).map(|entry| *entry)
    }

    #[allow(unused)]
    pub fn print_latency_histogram(&self) {
        let mut histogram: BTreeMap<u128, u32> = BTreeMap::new();

        for entry in self.samples.iter() {
            if let Some(end) = entry.1 {
                let latency = end.duration_since(entry.0).as_micros();
                *histogram.entry(latency).or_insert(0) += 1;
            }
        }

        println!("Latency Histogram:");
        for (latency, count) in histogram {
            println!("{:<10} µs | {}", latency, "*".repeat(count as usize));
        }
    }

    #[allow(unused)]
    pub fn mean_latency(&self) -> Option<Duration> {
        let mut total_latency = 0;
        let mut count = 0;

        for entry in self.samples.iter() {
            if let Some(end) = entry.1 {
                total_latency += end.duration_since(entry.0).as_nanos();
                count += 1;
            }
        }

        if count > 0 {
            Some(Duration::from_nanos((total_latency / count) as u64))
        } else {
            None
        }
    }

    #[allow(unused)]
    pub fn quantile_latency(&self, quantile: f64) -> Option<Duration> {
        let mut latencies: Vec<u128> = self
            .samples
            .iter()
            .filter_map(|entry| entry.1.map(|end| end.duration_since(entry.0).as_nanos()))
            .collect();

        if latencies.is_empty() {
            return None;
        }

        latencies.sort_unstable();
        let index = (latencies.len() as f64 * quantile).ceil() as usize - 1;
        Some(Duration::from_nanos(
            latencies[index.min(latencies.len() - 1)] as u64,
        ))
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
}
/* collector.rs ends here */
