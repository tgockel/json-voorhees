FROM ubuntu:17.10
MAINTAINER Travis Gockel <travis@gockelhut.com>

RUN apt-get update          \
 && apt-get install --yes   \
    cmake                   \
    grep                    \
    g++-7                   \
    lcov                    \
    libboost-all-dev        \
    ninja-build

RUN update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-7 99

CMD ["/root/jsonv/config/run-tests"]
