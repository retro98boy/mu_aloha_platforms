# Build from the repository root:
# docker buildx build --load -f custcom-build/Dockerfile -t retro98boy/mu-aloha-platforms-build .
# This environment successfully builds an Aloha image from commit
# 78ff39135c65cbdcf353db277ffeea61f965698e (November 6, 2026).

FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-e", "-o", "pipefail", "-c"]

# Install locale, timezone and user-management tools.
RUN sed -i -e 's|archive.ubuntu.com|mirrors.ustc.edu.cn|g' \
           -e 's|security.ubuntu.com|mirrors.ustc.edu.cn|g' \
           /etc/apt/sources.list.d/ubuntu.sources \
 && apt-get update \
 && apt-get install -y --no-install-recommends locales tzdata sudo ca-certificates curl

RUN ln -snf /usr/share/zoneinfo/Asia/Shanghai /etc/localtime \
 && sed -i -e 's/^# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' \
           -e 's/^# zh_CN.UTF-8 UTF-8/zh_CN.UTF-8 UTF-8/' /etc/locale.gen \
 && locale-gen \
 && echo 'LANG=en_US.UTF-8' > /etc/locale.conf
ENV TZ=Asia/Shanghai LANG=en_US.UTF-8

# Ubuntu 24.04 provides the ubuntu user with UID/GID 1000.
RUN usermod -aG sudo ubuntu \
 && passwd -d ubuntu \
 && echo 'ubuntu ALL=(ALL) NOPASSWD: ALL' > /etc/sudoers.d/ubuntu \
 && chmod 0440 /etc/sudoers.d/ubuntu

# AArch64 EDK II, BootShim and firmware packaging dependencies.
RUN apt-get install -y --no-install-recommends \
    git build-essential uuid-dev clang llvm lld gcc-aarch64-linux-gnu \
    iasl nasm mono-devel gettext \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/*

# Pin the Python toolchain used by the validated Aloha build environment.
RUN curl -fsSL --retry 3 https://astral.sh/uv/0.12.13/install.sh -o /tmp/install-uv.sh \
 && UV_INSTALL_DIR=/usr/local/bin UV_NO_MODIFY_PATH=1 bash /tmp/install-uv.sh \
 && rm /tmp/install-uv.sh
ENV UV_PYTHON_INSTALL_DIR=/opt/mu-aloha-platforms/python \
    UV_PYTHON_PREFERENCE=only-managed \
    UV_SYSTEM_CERTS=true \
    VIRTUAL_ENV=/opt/mu-aloha-platforms/venv
ENV PATH="/opt/mu-aloha-platforms/venv/bin:${PATH}" \
    CLANGPDB_BIN=/usr/bin/ \
    CLANGPDB_AARCH64_PREFIX=aarch64-linux-gnu-
COPY pip-requirements.txt /tmp/pip-requirements.txt
RUN uv python install 3.10.21 \
 && uv venv --python 3.10.21 --seed "$VIRTUAL_ENV" \
 && uv pip install --python "$VIRTUAL_ENV/bin/python" -r /tmp/pip-requirements.txt \
 && uv pip check --python "$VIRTUAL_ENV/bin/python" \
 && uv cache clean

COPY scripts/install_avbtool.sh /tmp/install_avbtool.sh
RUN bash /tmp/install_avbtool.sh \
 && curl -fsSL --retry 3 -o /usr/local/bin/nuget.exe \
    https://dist.nuget.org/win-x86-commandline/latest/nuget.exe \
 && printf '#!/bin/sh\nexec mono /usr/local/bin/nuget.exe "$@"\n' > /usr/local/bin/nuget \
 && chmod 755 /usr/local/bin/nuget

RUN mkdir -p /workspace/mu-aloha-platforms \
 && chown -R ubuntu:ubuntu /workspace/mu-aloha-platforms /opt/mu-aloha-platforms/python /opt/mu-aloha-platforms/venv
WORKDIR /workspace/mu-aloha-platforms
USER ubuntu
CMD ["/bin/bash"]
