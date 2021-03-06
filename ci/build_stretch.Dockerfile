FROM debian:stretch

RUN apt-get update && apt-get -y dist-upgrade && apt-get -y --no-install-recommends install wget gnupg && \
    wget http://pkg.yeti-switch.org/key.gpg -O - | apt-key add -

RUN echo "deb http://pkg.yeti-switch.org/debian/stretch unstable main ext" >> /etc/apt/sources.list && \
    echo "deb http://deb.debian.org/debian buster main contrib non-free" >> /etc/apt/sources.list && \
    echo "Package: *\nPin: release n=buster\nPin-Priority: 50\n\nPackage: python-git python-gitdb python-smmap python-tzlocal\nPin: release n=buster\nPin-Priority: 500\n\n" | tee /etc/apt/preferences && \
    wget --no-check-certificate https://www.postgresql.org/media/keys/ACCC4CF8.asc -O - | apt-key add - && \
    echo "deb http://apt.postgresql.org/pub/repos/apt/ stretch-pgdg main" >> /etc/apt/sources.list


RUN apt-get update && apt-get -y --no-install-recommends install build-essential devscripts \
    ca-certificates apt-transport-https debhelper fakeroot lintian python-jinja2 \
    git-changelog python-setuptools lsb-release curl cmake g++ gcc libcurl4-openssl-dev libcurl3 libcurl3-dev libpqxx-dev libnanomsg-dev libconfuse-dev libyeticc-dev libssl-dev zlib1g-dev

ADD . /build/yeti-lb/

WORKDIR /build/yeti-lb/
CMD mkdir build && cd build && cmake .. && make && make package-server

