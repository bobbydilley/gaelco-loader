FROM ubuntu:22.04 AS gaelco-loader-build

WORKDIR /gaelco-loader

COPY . .

RUN ls
