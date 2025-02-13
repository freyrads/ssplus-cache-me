FROM archlinux:base as init

RUN pacman -Sy --needed --noconfirm openssl reflector && reflector --save /etc/pacman.d/mirrorlist && \
      pacman -Syu --needed --noconfirm libc++ sqlite3

FROM init as build

# Build dependencies
WORKDIR /app

# Copy source files
COPY cmake ./cmake
COPY include ./include
COPY src ./src
COPY libs ./libs
COPY CMakeLists.txt ./

# Install dependencies
RUN pacman -Syu --needed --noconfirm base-devel libc++ cmake clang nlohmann-json

RUN mkdir -p build && cd build && \
      export CC=clang && \
      export CXX=clang++ && \
      export LDFLAGS='-flto -stdlib=libc++ -lc++' && \
      export CFLAGS='-flto' && \
      export CXXFLAGS='-flto -stdlib=libc++' && \
      cmake .. -DDEBUG=OFF -DEXTERNAL_JSON=ON -DSS_COMP=ON && make all -j4

FROM init as deploy

RUN groupadd ssplus && useradd -m -g ssplus ssplus

USER ssplus

WORKDIR /home/ssplus

COPY --chown=ssplus:ssplus --from=build \
             /app/build/ssplus-cache-me \
             /home/ssplus/

VOLUME ["/home/ssplus/db"]

WORKDIR /home/ssplus/db

CMD ../ssplus-cache-me
