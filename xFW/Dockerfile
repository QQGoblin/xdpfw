FROM ubuntu:24.04

RUN apt-get update \
    && apt-get install -y iproute2 jq tini vim-tiny \
        libbpf1 libboost-program-options1.83.0 libflatbuffers2 \
        libgrpc++1.51t64 libspdlog1.12 libmaxminddb0 \
    && rm -rf /var/lib/apt/lists/*

COPY bin/bpftool   /usr/local/bin/bpftool
COPY bin/tempesta  /opt/tempesta

WORKDIR /opt/tempesta
CMD [ "/bin/bash"]
