ARG project_location
FROM ruby:2.7.2
RUN curl -sS https://dl.yarnpkg.com/debian/pubkey.gpg |  apt-key add - && \
echo "deb https://dl.yarnpkg.com/debian/ stable main" | sudo tee /etc/apt/sources.list.d/yarn.list && \
apt update && apt install -y yarn
RUN apt install sudo && \
bash < <(curl -sL https://raw.github.com/railsgirls/installation-scripts/master/rails-install-ubuntu.sh) && \
gem install rails && \
apt purge sudo
RUN mkdir /app
COPY ${project_location} /app
RUN cd /app && \
bundle update && \
bundle install
ENTRYPOINT [ "rails" ]
