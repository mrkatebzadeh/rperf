-------------------------------------------------------------------------------

# RPerf: Accurate Latency Measurement for RDMA #

-------------------------------------------------------------------------------

**Table of Contents**

- [RPerf](#rperf)
- [Prerequisites](#prerequisites)
- [Install](#install)
- [Configuration](#configuration)
- [Running Tests](#running-tests)
- [Contacts](#contacts)

## RPerf ##

This package provides an accurate benchmark tools for **RDMA**-based networks.

## Prerequisites ##

Before you install RPerf, you must have the following libraries:

- cmake
- libncurses5-dev
- rdma-core libibverbs1 librdmacm1 libibmad5 libibumad3 librdmacm1 ibverbs-providers rdmacm-utils infiniband-diags libfabric1 ibverbs-utils libibverbs-dev

## Install ##

Clone the repository:
```
git clone https://github.com/mrkatebzadeh/rperf_c.git
```
Then you can simply make the package:
```
cd rperf_c
mkdir build
cd build
cmake .. && make
```

## Configuration ##

RPerf by default locates *rdmarc* file in the working directory. This file contains test parameters. To start, copy the template of config file into build directory:
```
cp ../rdmarc .
```
Then change the parameters according to what you desire. 

## Running Tests ##
The simplest way to run with default settings, on the server and clients:
```
./rperf_c 
```
Make sure *rdmarc* file on each node has the proper values for __is_server__ and __server_name__ parameters.


## Contacts ##

This implementation is a research prototype that shows the feasibility of accurate latency measurement and has been tested on a cluster equipped with _Mellanox MT27700 ConnectX-4_ HCAs and a _Mellanox SX6012_ IB switch. It is NOT production quality code. The technical details can be found [here](https://ease-lab.github.io/ease_website/pubs/RPERF_ISPASS20.pdf). If you have any questions, please raise issues on Github or contact the authors below.

[M.R. Siavash Katebzadeh](http://mr.katebzadeh.xyz) (m.r.katebzadeh@ed.ac.uk)
<!-- markdown-toc end -->

 