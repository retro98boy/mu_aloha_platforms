# Run from the mu_aloha_platforms repository root:
# docker build -f custom-build/Dockerfile -t retro98boy/mu-aloha-dev:latest .
# docker run --rm -it -v "$PWD:/workspace" retro98boy/mu-aloha-dev:latest

FROM ghcr.io/astral-sh/uv:latest AS uv
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-e", "-o", "pipefail", "-c"]

# Keep the locale, user and workspace conventions of rockchip-android13-dev.
RUN sed -i -e 's|http://archive.ubuntu.com/ubuntu/|http://mirrors.ustc.edu.cn/ubuntu/|g' \
           -e 's|http://security.ubuntu.com/ubuntu/|http://mirrors.ustc.edu.cn/ubuntu/|g' \
           /etc/apt/sources.list.d/ubuntu.sources \
 && apt-get update \
 && apt-get install -y --no-install-recommends ca-certificates locales tzdata sudo zsh vim \
 && sed -i 's|http://mirrors.ustc.edu.cn/|https://mirrors.ustc.edu.cn/|g' /etc/apt/sources.list.d/ubuntu.sources \
 && ln -snf /usr/share/zoneinfo/Asia/Shanghai /etc/localtime \
 && sed -i -e 's/^# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' \
           -e 's/^# zh_CN.UTF-8 UTF-8/zh_CN.UTF-8 UTF-8/' /etc/locale.gen \
 && locale-gen

ENV TZ=Asia/Shanghai LANG=en_US.UTF-8

# Ubuntu 24.04 already provides the ubuntu user with UID/GID 1000.
RUN usermod -s /bin/zsh -aG sudo ubuntu \
 && echo 'ubuntu:ubuntu' | chpasswd \
 && echo 'ubuntu ALL=(ALL) NOPASSWD: ALL' > /etc/sudoers.d/ubuntu \
 && chmod 0440 /etc/sudoers.d/ubuntu

# Use the project's dependency installer, including its version-script inputs.
# These copies are image-local; the mounted checkout is not modified.
WORKDIR /opt/aloha-setup
COPY build_setup.sh timebuild.sh ./
COPY Platforms/AndromedaPkg/Andromeda.dsc.inc ./Platforms/AndromedaPkg/
RUN bash -e build_setup.sh \
 && test -s /usr/local/bin/nuget.exe \
 && printf '#!/bin/sh\nexec mono /usr/local/bin/nuget.exe "$@"\n' > /usr/local/bin/nuget \
 && chmod +x /usr/local/bin/nuget \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/*

# Use distribution LLVM rather than build_setup.sh's hard-coded llvm-38 path.
ENV CLANGPDB_BIN=/usr/bin/ CLANGPDB_AARCH64_PREFIX=aarch64-linux-gnu-

COPY --from=uv /uv /uvx /usr/local/bin/
ENV UV_PYTHON_INSTALL_DIR=/opt/uv-python \
    UV_PYTHON_PREFERENCE=only-managed \
    UV_NATIVE_TLS=true \
    VIRTUAL_ENV=/opt/aloha-venv
ENV PATH="/opt/aloha-venv/bin:${PATH}"

COPY pip-requirements.txt ./
RUN uv python install 3.10 \
 && uv venv --python 3.10 --seed "$VIRTUAL_ENV" \
 && uv pip install --python "$VIRTUAL_ENV/bin/python" -r pip-requirements.txt \
 && "$VIRTUAL_ENV/bin/python" -m pip check \
 && chown -R ubuntu:ubuntu /opt/aloha-venv /opt/uv-python \
 && uv cache clean

# UEFIReader targets .NET 8. Keep the SDK available for development as well.
ARG UEFIREADER_REF=fc2e9e3a1877cdcc7fe48d10a72a1a5845ce0850
# Reuse identical FFS modules; retain diagnostics and fail on other conflicts.
COPY custom-build/uefireader-conflict-diagnostics.patch /opt/uefireader-conflict-diagnostics.patch
RUN apt-get update \
 && apt-get install -y --no-install-recommends dotnet-sdk-8.0 \
 && git clone https://github.com/MSMTools/UEFIReader.git /opt/UEFIReader-src \
 && git -C /opt/UEFIReader-src checkout --detach "$UEFIREADER_REF" \
 && git -C /opt/UEFIReader-src apply --check /opt/uefireader-conflict-diagnostics.patch \
 && git -C /opt/UEFIReader-src apply /opt/uefireader-conflict-diagnostics.patch \
 && dotnet publish /opt/UEFIReader-src/UEFIReader/UEFIReader.csproj \
      --configuration Release --output /opt/UEFIReader --self-contained false \
      -p:UseAppHost=false \
 && printf '#!/bin/sh\nexec /usr/bin/dotnet /opt/UEFIReader/UEFIReader.dll "$@"\n' > /usr/local/bin/uefireader \
 && chmod +x /usr/local/bin/uefireader \
 && ln -s /usr/local/bin/uefireader /usr/local/bin/UEFIReader \
 && chown -R ubuntu:ubuntu /opt/UEFIReader-src \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /workspace && chown ubuntu:ubuntu /workspace
WORKDIR /workspace
USER ubuntu

RUN uefireader

RUN git config --global --add safe.directory /workspace \
 && git config --global --add safe.directory '/workspace/*'

CMD ["/bin/bash"]
