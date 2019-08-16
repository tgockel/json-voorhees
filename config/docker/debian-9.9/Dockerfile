FROM debian:9.9
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
