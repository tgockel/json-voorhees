FROM ubuntu:19.10
LABEL maintainer="Travis Gockel <travis@gockelhut.com>"

RUN apt-get update          \
 && apt-get install --yes   \
    cmake                   \
    grep                    \
    g++                     \
    lcov                    \
    ninja-build

CMD ["/root/jsonv/config/run-tests"]
