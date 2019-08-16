FROM opensuse/leap:15.1
LABEL maintainer="Travis Gockel <travis@gockelhut.com>"

RUN zypper refresh                  \
 && zypper install -y               \
    boost-devel                     \
    libboost_filesystem1_66_0-devel \
    cmake                           \
    grep                            \
    gcc-c++                         \
    git                             \
    lcov                            \
    ninja                           \
    rpm-build

CMD ["/root/jsonv/config/run-tests"]
