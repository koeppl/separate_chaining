# vim: set syntax=dockerfile:

FROM debian

COPY . /src
WORKDIR /src


RUN \
		apt-get update && \
		apt-get -qy dist-upgrade && \
		apt-get install -qy  && \
		apt-get install -qy cmake g++ git && \
		git submodule init &&\
		git submodule update &&\
		mkdir -p dockerbuild && \
		cd dockerbuild && \ 
		cmake .. && \
		make && \
		./example_map

