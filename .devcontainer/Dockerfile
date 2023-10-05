FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update -qq \
    && apt-get install -y \
# x86_64 / generic packages
      bash \
      build-essential \
      cmake \
      git \
      make \
      python3 \
      python3-pip \
      python-is-python3 \
      tar \
      unzip \
      wget \
      curl \
      # aarch64 packages
      libffi-dev \
      libssl-dev \
      python3-dev \
      git \
      apt-utils \
      pkg-config \
      libpixman-1-dev \
      libcairo2-dev \
      libpango-1.0-0 \
      ibpango1.0-dev \
      libpangocairo-1.0-0 \
    && curl -sL https://deb.nodesource.com/setup_18.x | bash - \
    && apt-get install -y nodejs \
    && rm -rf /var/cache/apt/* /var/lib/apt/lists/*;

# Git needed for PROJECT_GIT_COMMIT_HASH variable setting

RUN pip3 install adafruit-nrfutil
RUN pip3 install -Iv cryptography==3.3
RUN pip3 install cbor

# build.sh knows how to compile
COPY build.sh /opt/

# Lets get each in a separate docker layer for better downloads
# GCC
RUN bash -c "source /opt/build.sh; GetGcc;"
# NrfSdk
RUN bash -c "source /opt/build.sh; GetNrfSdk;"
# McuBoot
RUN bash -c "source /opt/build.sh; GetMcuBoot;"

###ENV SOURCES_DIR /sources
###CMD ["/opt/build.sh"]

RUN chmod a+rx /opt/build.sh

### Added
RUN npm install -g typescript@5.0.4

RUN npm i ts-node@10.9.1 -g
###RUN npm i @swc/core -g
###RUN npm i @swc/core@1.3.88 -g
RUN npm i @swc/core@1.3.82 -g
RUN npm i lv_img_conv@0.3.0 -g
RUN npm i lv_font_conv@1.5.2 -g

RUN adduser infinitime

#ENV SOURCES_DIR /workspaces/InfiniTime
# .devcontainer.json -- "containerEnv": { "SOURCES_DIR": "${containerWorkspaceFolder}" },

ENV ARM_NONE_EABI_TOOLCHAIN_PATH /opt/gcc-arm-none-eabi-10.3-2021.10
ENV NRF5_SDK_PATH /opt/nRF5_SDK_15.3.0_59ac345

# /opt/build.sh pinetime-app

# [ "$(basename $PWD)" == 'build' ] || cd build; cmake .. -DARM_NONE_EABI_TOOLCHAIN_PATH=$ARM_NONE_EABI_TOOLCHAIN_PATH -DNRF5_SDK_PATH=$NRF5_SDK_PATH
# make
