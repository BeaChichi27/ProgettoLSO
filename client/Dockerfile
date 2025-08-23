FROM ubuntu:24.04

# Installa dipendenze
RUN apt-get update && \
    apt-get install -y \
    build-essential \
    gcc \
    make \
    iputils-ping \
    netcat-openbsd \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copia i file sorgente
COPY src/ ./src/
COPY Makefile .

# Compila il client per Linux  
RUN sed -i 's/-lws2_32//g' Makefile && \
    sed -i 's/\.exe//g' Makefile && \
    make clean && make

# Script per aspettare che il server sia pronto
RUN echo '#!/bin/bash\n\
SERVER_HOST=${SERVER_HOST:-localhost}\n\
SERVER_PORT=${SERVER_PORT:-8080}\n\
echo "Aspettando che il server $SERVER_HOST:$SERVER_PORT sia pronto..."\n\
while ! nc -z $SERVER_HOST $SERVER_PORT; do\n\
    echo "Server non ancora pronto, aspetto 2 secondi..."\n\
    sleep 2\n\
done\n\
echo "Server pronto! Avvio client..."\n\
exec ./client\n\
' > /app/start_client.sh && chmod +x /app/start_client.sh

# Usa lo script di avvio invece del client diretto
CMD ["/app/start_client.sh"]