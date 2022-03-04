FROM archlinux
LABEL maintainer="Travis Gockel <travis@gockelhut.com>"

RUN pacman -Sy --noconfirm  \
    cmake                   \
    gcc                     \
    git                     \
    grep                    \
    make                    \
    ninja

CMD ["/root/jsonv/config/run-tests"]
