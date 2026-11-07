FROM ghcr.io/edgetx/edgetx-dev:2.12 AS build
COPY . /src
WORKDIR /build
RUN git config --global --add safe.directory '*' \
 && cmake -DPCB=X10 -DPCBREV=TX16S -DCMAKE_BUILD_TYPE=Release -Wno-dev /src \
 && cmake --build . --target native-configure \
 && cmake --build native --target vradio --parallel 4

FROM ghcr.io/edgetx/edgetx-dev:2.12
COPY --from=build /build/native/vradio /build/native/libedgetx-tx16s-simulator.so /opt/vradio/
COPY tools/vradio/sd /opt/vradio/sd
COPY tools/vradio/start.sh /opt/vradio/start.sh
ENV LD_LIBRARY_PATH=/opt/vradio
EXPOSE 5770 5771
ENTRYPOINT ["/opt/vradio/start.sh"]
