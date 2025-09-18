# Loopback Latency Test with RPerf

This example demonstrates how to test loopback latency using RPerf by running two instances: one as an agent and the other as a probe.

## Prerequisites
- Ensure you have Rust and Cargo installed on your system.

## Setup

1. **Build RPerf**
   ```bash
   cargo build --release
   ```

2. **Navigate to the Example Directory**
   ```bash
   cd example
   ```

3. **Prepare Configurations**
   - Ensure you have the following configuration files:
      - `agent_conf.toml`: Configuration for the agent.
      - `probe_conf.toml`: Configuration for the probe.

4. **Run RPerf as Agent**
   Open a terminal and execute:
   ```bash
   ../target/release/rperf --config agent_conf.toml
   ```

5. **Run RPerf as Probe**
   Open another terminal and execute:
   ```bash
   ../target/release/rperf --config probe_conf.toml
   ```

## Testing Loopback Latency

RPerf will now run with one instance as an agent and another as a probe, allowing you to test the loopback latency between the two instances.

## Additional Configuration

You can modify the `agent_conf.toml` and `probe_conf.toml` files to adjust the test parameters according to your needs.
