FROM mcr.microsoft.com/devcontainers/cpp:1-ubuntu-24.04

ARG REINSTALL_CMAKE_VERSION_FROM_SOURCE="3.22.2"

# Optionally install the cmake for vcpkg
COPY ./reinstall-cmake.sh /tmp/

RUN if [ "${REINSTALL_CMAKE_VERSION_FROM_SOURCE}" != "none" ]; then \
        chmod +x /tmp/reinstall-cmake.sh && /tmp/reinstall-cmake.sh ${REINSTALL_CMAKE_VERSION_FROM_SOURCE}; \
    fi \
    && rm -f /tmp/reinstall-cmake.sh

#install vulkan sdk
RUN wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo tee /etc/apt/trusted.gpg.d/lunarg.asc \
    && sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-noble.list http://packages.lunarg.com/vulkan/lunarg-vulkan-noble.list \
    && sudo apt -y update \
    && sudo apt -y install vulkan-sdk \
    linux-headers-$(uname -r)

# install gpu drivers
RUN sudo apt-get update && sudo apt-get -y upgrade && \
    sudo apt-get -y install --no-install-recommends \
    software-properties-common

RUN sudo apt -y install \
    libxinerama-dev \
    libxcursor-dev \
    xorg-dev \
    libglu1-mesa-dev \
    pkg-config\
    build-essential\
    autoconf \
    automake \
    libtool \
    xvfb \
    python3 \
    python3-pip\
    python3-venv

# [Optional] Uncomment this section to install additional vcpkg ports.
# RUN su vscode -c "${VCPKG_ROOT}/vcpkg install <your-port-name-here>"

# [Optional] Uncomment this section to install additional packages.
# RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
#     && apt-get -y install --no-install-recommends <your-package-list-here>

CMD ["xvfb", "-screen", "0", "640x480x24", "-fb", "0", "&", "xeyes"]