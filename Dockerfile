FROM ubuntu:latest

WORKDIR /opt/rperf

RUN apt-get -y update && DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC apt-get install -y sudo git gcc g++ cmake make libibverbs-dev curl gcovr lcov llvm pkg-config clang-format clang-tidy cppcheck iwyu
RUN useradd -r  rperf && echo "rperf:rperf" | chpasswd && adduser rperf sudo
RUN groupmod -o -g 1000 rperf
RUN usermod -o -u 1000 rperf
USER rperf

EXPOSE 8585

CMD ["bash"]

LABEL Name=rperf Version=0.0.1
