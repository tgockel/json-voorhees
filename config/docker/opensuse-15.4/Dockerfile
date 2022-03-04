FROM opensuse/leap:15.4
LABEL maintainer="Travis Gockel <travis@gockelhut.com>"

RUN zypper refresh                  \
 && zypper install -y               \
    boost-devel                     \
    cmake                           \
    grep                            \
    gcc-c++                         \
    git                             \
    lcov                            \
    ninja                           \
    rpm-build

CMD ["/root/jsonv/config/run-tests"]
