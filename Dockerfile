FROM debian:stretch-slim
LABEL maintainer="lamelizard"

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
    sudo build-essential cmake \
    xorg-dev libgl1-mesa-dev libasound2-dev

ADD . / project/

WORKDIR /build

RUN cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ../project \
    && make -j8 \
    && make package

WORKDIR /

CMD ["/bin/bash"]

# apt install docker.io
# build with:
# docker build -t myname .
# run shell (exit shell with "exit"):
# docker run -it myname
# copy out with 
# docker run --name myname2 myname
# docker cp myname2:/build/psychokinesis.tar.gz .
# clean docker
# docker stop $(docker ps -aq)
# docker rm $(docker ps -aq)
# docker rmi $(docker images -q)
