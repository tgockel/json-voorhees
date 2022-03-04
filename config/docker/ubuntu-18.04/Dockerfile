FROM ubuntu:18.04
LABEL maintainer="Travis Gockel <travis@gockelhut.com>"

RUN apt-get update                  \
 && DEBIAN_FRONTEND=noninteractive  \
    apt-get install --yes           \
    cmake                           \
    grep                            \
    g++                             \
    lcov                            \
    ninja-build                     \
 && rm -rf /var/lib/apt/lists/*

CMD ["/root/jsonv/config/run-tests"]
