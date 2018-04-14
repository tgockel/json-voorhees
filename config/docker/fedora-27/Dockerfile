FROM fedora:27
MAINTAINER Travis Gockel <travis@gockelhut.com>

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

RUN update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-7 99

CMD ["/root/jsonv/config/run-tests"]

