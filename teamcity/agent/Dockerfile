FROM jetbrains/teamcity-minimal-agent:latest

RUN apt-get install -y \
    iproute2 \
    iputils-ping

RUN apt-get install -y git

# Install ant (from binary package to avoid pulling in OpenJDK 11)
RUN apt-get install -y wget unzip
RUN cd /opt && wget http://apache.mirrors.pair.com//ant/binaries/apache-ant-1.10.6-bin.zip
RUN cd /opt && unzip apache-ant-1.10.6-bin.zip && rm apache-ant-1.10.6-bin.zip
ENV PATH="/opt/apache-ant-1.10.6/bin:${PATH}"

# Install clang
RUN apt-get install -y clang

# Install buck
RUN apt-get install -y python git
RUN cd /opt && git clone https://github.com/facebook/buck
RUN cd /opt/buck && ant default
ENV PATH="/opt/buck/bin:${PATH}"

# Install python3
RUN apt-get install -y python3
RUN apt-get install -y python3-distutils

# Install docker CLI
RUN apt-get install -y \
    apt-transport-https \
    ca-certificates \
    curl \
    gnupg-agent \
    software-properties-common
RUN curl -fsSL https://download.docker.com/linux/ubuntu/gpg | apt-key add
RUN apt-key fingerprint 0EBFCD88
RUN add-apt-repository \
   "deb [arch=amd64] https://download.docker.com/linux/ubuntu \
   $(lsb_release -cs) \
   stable"
RUN apt-get update
RUN apt-get install docker-ce-cli