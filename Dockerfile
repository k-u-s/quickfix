FROM python:3-alpine as build

RUN apk add --no-cache \
    build-base autoconf automake openssl-dev libtool clang swig python3-dev

WORKDIR /build

COPY . /build/

WORKDIR /build/
RUN ./bootstrap
RUN ./configure --with-python3 --with-openssl --prefix=/publish
# alpine linux has some issue with linked files. So overwrite links with original files
RUN cp src/python/quick*.py src/python3/
RUN cp src/python/Quick*.cpp src/python3/
RUN cp src/python/Quick*.h src/python3/
RUN make
RUN make DESTDIR=/dest install

FROM python:3-alpine as runtime

RUN apk update
# build-base is needed in order for pip to install uvicorn
# swig required by custom quickfix
RUN apk add --no-cache \
    python3 curl build-base swig

# Coping installed files by make
COPY --from=build /dest/publish/lib/ /usr/local/lib/
COPY --from=build /dest/usr/local/lib/python*/site-packages/* /usr/local/lib/python/quickfix/

RUN pip install "uvicorn == 0.11.8"

RUN apk del build-base

ENV PYTHONPATH="/usr/local/lib/python/quickfix/:${PYTHONPATH}"
