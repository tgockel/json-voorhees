FROM ubuntu:19.04
LABEL maintainer="Travis Gockel <travis@gockelhut.com>"

RUN apt-get update          \
 && apt-get install --yes   \
    cmake                   \
    grep                    \
    g++                     \
    lcov                    \
    libboost-all-dev        \
    ninja-build

CMD ["/root/jsonv/config/run-tests"]
