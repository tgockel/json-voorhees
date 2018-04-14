FROM opensuse:42.3
MAINTAINER Travis Gockel <travis@gockelhut.com>

RUN zypper refresh          \
 && zypper install -y       \
    boost-devel             \
    cmake                   \
    grep                    \
    gcc7-c++                \
    git                     \
    lcov                    \
    ninja                   \
    rpm-build

RUN update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-7 99

CMD ["/root/jsonv/config/run-tests"]
