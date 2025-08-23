FROM ubuntu:24.04

# Installa dipendenze per la compilazione
RUN apt-get update && \
    apt-get install -y \
    build-essential \
    gcc \
    make \
    gdb \
    net-tools \
    iputils-ping \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copia i file sorgente
COPY src/ ./src/
COPY Makefile .

# Compila il server per Linux
RUN sed -i 's/-lws2_32//g' Makefile && \
    sed -i 's/\.exe//g' Makefile && \
    make clean && make

# Espone la porta del server (configurabile)
EXPOSE ${PORT:-8080}

# Aggiungi healthcheck per verificare che il server sia pronto
HEALTHCHECK --interval=5s --timeout=3s --start-period=10s \
    CMD netstat -ln | grep :${PORT:-8080} || exit 1

# Avvia il server
CMD ["./server"]