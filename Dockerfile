# apt install docker.io
# build with:
# docker build -t myname .
# run shell (exit shell with "exit"):
# docker run -it myname
FROM debian:stretch-slim
LABEL maintainer="lamelizard"

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
    sudo build-essential cmake xorg-dev libgl1-mesa-dev

ADD . / project/

WORKDIR /build

RUN cmake -DCMAKE_BUILD_TYPE=Release ../project \
    && cmake --build . 

WORKDIR /

#VOLUME /build
CMD ["/bin/bash"]
