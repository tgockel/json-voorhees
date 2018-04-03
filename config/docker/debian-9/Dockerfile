FROM debian:9
MAINTAINER Travis Gockel <travis@gockelhut.com>

RUN apt-get update          \
 && apt-get install --yes   \
    cmake                   \
    grep                    \
    g++-6                   \
    lcov                    \
    libboost-all-dev        \
    ninja-build

RUN update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-6 99

CMD ["/root/jsonv/config/run-tests"]
