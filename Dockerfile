#LABEL maintainer="skycoder42.de@gmx.de"
FROM debian:latest

COPY . /tmp/src
RUN /tmp/src/tools/appserver/dockerbuild/install.sh

# create env vars & ready for usage
ENV QDSAPP_CONFIG_FILE=/etc/qdsapp.conf
# must be set: ENV QDSAPP_DATABASE_HOST=
ENV QDSAPP_DATABASE_PORT=5432
ENV QDSAPP_DATABASE_USER=postgres
# must be set: ENV QDSAPP_DATABASE_PASSWORD=
ENV QDSAPP_DATABASE_NAME=$QDSAPP_DATABASE_USER

CMD ["/usr/bin/env_start.sh"]
