FROM ubuntu:20.04
LABEL maintainer="Travis Gockel <travis@gockelhut.com>"

RUN apt-get update                  \
 && DEBIAN_FRONTEND=noninteractive  \
    apt-get install --yes           \
    cmake                           \
    doxygen                         \
    graphviz                        \
    grep                            \
    g++                             \
    lcov                            \
    ninja-build                     \
    texlive-full

CMD ["/root/jsonv/config/make-doxygen"]
