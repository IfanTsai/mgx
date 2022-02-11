FROM ubuntu:latest as builder
WORKDIR /mgx
RUN apt update && apt install -y g++ make
COPY . .
RUN sed -i "s/DEBUG = true/DEBUG = false/g" config.mk
RUN make -j$(cat /proc/cpuinfo | grep "processor" | wc -l)

FROM ubuntu:latest
WORKDIR /mgx
COPY --from=builder /mgx/http/html ./http/html
COPY --from=builder /mgx/mgx.conf .
COPY --from=builder /mgx/mgx .

EXPOSE 8081/tcp
EXPOSE 8082/tcp
CMD [ "/mgx/mgx" ]