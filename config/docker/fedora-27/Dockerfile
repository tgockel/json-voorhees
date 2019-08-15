FROM fedora:27
LABEL maintainer="Travis Gockel <travis@gockelhut.com>"

RUN dnf refresh             \
 && dnf install -y          \
    boost-devel             \
    cmake                   \
    grep                    \
    gcc-c++                 \
    git                     \
    lcov                    \
    make                    \
    ninja-build             \
    rpm-build

CMD ["/root/jsonv/config/run-tests"]
