FROM archlinux/base
LABEL maintainer="Travis Gockel <travis@gockelhut.com>"

RUN pacman -Sy --noconfirm  \
    boost                   \
    cmake                   \
    gcc                     \
    git                     \
    grep                    \
    make                    \
    ninja

CMD ["/root/jsonv/config/run-tests"]
