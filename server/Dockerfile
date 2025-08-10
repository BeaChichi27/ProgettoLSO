FROM ubuntu:24.04

RUN apt-get update && \
    apt-get install -y \
    build-essential \
    gcc \
    make \
    gdb \
    net-tools

WORKDIR /app

COPY src/ ./src/
COPY Makefile .

# Correzione: Aggiungi i flag necessari per la compilazione su Linux
RUN sed -i 's/-lws2_32//g' Makefile && \
    sed -i 's/\.exe//g' Makefile && \
    make

EXPOSE 8080

CMD ["./server"]