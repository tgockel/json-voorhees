FROM ubuntu:14.04
LABEL maintainer="Travis Gockel <travis@gockelhut.com>"

RUN apt-get update \
 && apt-get install --yes software-properties-common \
 && add-apt-repository --yes ppa:ubuntu-toolchain-r/test \
 && add-apt-repository --yes ppa:george-edison55/cmake-3.x

RUN apt-get update          \
 && apt-get install --yes   \
    cmake                   \
    grep                    \
    g++-4.9                 \
    lcov                    \
    libboost-all-dev        \
    ninja-build             \
 && update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-4.9 99

CMD ["/root/jsonv/config/run-tests"]
