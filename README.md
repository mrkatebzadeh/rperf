-------------------------------------------------------------------------------

# 🚀 RPerf: Accurate Latency Measurement Framework for RDMA 📈

-------------------------------------------------------------------------------


## 🎯 Overview

This package provides an accurate benchmarking tool for **RDMA**-based networks, implemented in Rust.

## 📋 Prerequisites

Before you install RPerf, you must have the following libraries:

- Rust (https://rustup.rs).
- libncurses5-dev
- rdma-core libibverbs1 librdmacm1 libibmad5 libibumad3 librdmacm1 ibverbs-providers rdmacm-utils infiniband-diags libfabric1 ibverbs-utils libibverbs-dev

## 🔧 Installation

Clone the repository:
```sh
git clone https://github.com/mrkatebzadeh/rperf.git
```
Then you can build the package using Cargo:
```sh
cargo build --release
```

## ⚙️ Configuration

RPerf by default locates a `config.toml` file in the working directory. This file contains test parameters. Change the parameters according to what you desire. See the [example](./example/README.md) for a simple loopback test.

## 🏃 Running Tests
The simplest way to run with default settings, on the server and clients:
```
./target/.../release/rperf
```
Make sure *config.toml* file on each node has the proper values for __is_agent__ and __server_addr__ parameters.


## 📚 Cite This Work

If you use RPerf in your research, please cite the following paper:

```bibtex
@INPROCEEDINGS{9238625,
  author={Katebzadeh, M. R. Siavash and Costa, Paolo and Grot, Boris},
  booktitle={2020 IEEE International Symposium on Performance Analysis of Systems and Software (ISPASS)},
  title={Evaluation of an InfiniBand Switch: Choose Latency or Bandwidth, but Not Both},
  year={2020},
  pages={180-191},
  doi={10.1109/ISPASS48437.2020.00033}
}
```

## 📞 Contacts

This implementation is a research prototype that shows the feasibility of accurate latency measurement and has been tested on a cluster equipped with _Mellanox MT27700 ConnectX-4_ HCAs and a _Mellanox SX6012_ IB switch. It is NOT production quality code. The technical details can be found [here](https://ease-lab.github.io/ease_website/pubs/RPERF_ISPASS20.pdf). If you have any questions, please raise issues on Github or contact the authors below.

[M.R. Siavash Katebzadeh](http://mr.katebzadeh.xyz) (mr@katebzadeh.xyz)
