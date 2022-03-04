FROM fedora:35
LABEL maintainer="Travis Gockel <travis@gockelhut.com>"

RUN dnf install -y --refresh    \
    cmake                       \
    grep                        \
    gcc-c++                     \
    git                         \
    lcov                        \
    make                        \
    ninja-build                 \
    rpm-build

CMD ["/root/jsonv/config/run-tests"]
