# -*- mode: ruby -*-
# vi: set ft=ruby :

# All Vagrant configuration is done below. The "2" in Vagrant.configure
# configures the configuration version (we support older styles for
# backwards compatibility). Please don't change it unless you know what
# you're doing.
Vagrant.configure(2) do |config|
  # The most common configuration options are documented and commented below.
  # For a complete reference, please see the online documentation at
  # https://docs.vagrantup.com.

  # Every Vagrant development environment requires a box. You can search for
  # boxes at https://atlas.hashicorp.com/search.
  config.vm.box = 'bento/ubuntu-16.04'

  # Disable automatic box update checking. If you disable this, then
  # boxes will only be checked for updates when the user runs
  # `vagrant box outdated`. This is not recommended.
  # config.vm.box_check_update = false

  # Create a forwarded port mapping which allows access to a specific port
  # within the machine from a port on the host machine. In the example below,
  # accessing "localhost:8080" will access port 80 on the guest machine.
  # config.vm.network "forwarded_port", guest: 80, host: 8080
  config.vm.network "forwarded_port", guest: 9090, host: 9090

  # Create a private network, which allows host-only access to the machine
  # using a specific IP.
  # config.vm.network 'private_network', ip: '10.30.0.100'

  # Create a public network, which generally matched to bridged network.
  # Bridged networks make the machine appear as another physical device on
  # your network.
  # config.vm.network "public_network"

  # Share an additional folder to the guest VM. The first argument is
  # the path on the host to the actual folder. The second argument is
  # the path on the guest to mount the folder. And the optional third
  # argument is a set of non-required options.
  # config.vm.synced_folder '.', '/opt/stun'

  # Provider-specific configuration so you can fine-tune various
  # backing providers for Vagrant. These expose provider-specific options.
  # Example for VirtualBox:
  #
  config.vm.provider "virtualbox" do |vb|
    # Customize the amount of memory on the VM:
    vb.memory = "2048"
  end
  #
  # View the documentation for the provider you are using for more
  # information on available options.

  # Define a Vagrant Push strategy for pushing to Atlas. Other push strategies
  # such as FTP and Heroku are also available. See the documentation at
  # https://docs.vagrantup.com/v2/push/atlas.html for more information.
  # config.push.define "atlas" do |push|
  #   push.app = "YOUR_ATLAS_USERNAME/YOUR_APPLICATION_NAME"
  # end

  config.ssh.forward_agent = true

  # Enable provisioning with a shell script. Additional provisioners such as
  # Puppet, Chef, Ansible, Salt, and Docker are also available. Please see the
  # documentation for more information about their specific syntax and use.
  config.vm.provision 'shell', inline: <<-SHELL
    sudo apt-get update
    sudo apt-get install -y default-jdk ant python git
    sudo apt-get install -y build-essential autoconf automake libtool pkg-config libssl-dev python-dev python3-dev

    pushd /opt

    # Clone watchman & buck
    for pkg in watchman buck; do
      if [[ ! -e $pkg ]]; then
        sudo mkdir -p $pkg
        sudo chown `whoami` $pkg
        echo $pkg
        git clone https://github.com/facebook/$pkg.git $pkg
      fi
    done

    # Compile watchman
    if ! watchman -v > /dev/null 2>&1; then
      pushd /opt/watchman
      ./autogen.sh
      ./configure
      make -j4
      sudo make install
    fi

    # Compile buck
    if ! buck --version > /dev/null 2>&1; then
      pushd /opt/buck
      ant
      sudo ln -sf $(pwd)/bin/buck /usr/local/bin/
      sudo ln -sf $(pwd)/bin/buckd /usr/local/bin/
    fi

    # Install node.js/npm/nuclide
    if ! node -v > /dev/null 2>&1; then
      pushd /tmp
      curl -sL https://deb.nodesource.com/setup_6.x -o nodesource_setup.sh
      sudo bash nodesource_setup.sh
      sudo apt-get install nodejs
      sudo npm install -g nuclide
    fi
  SHELL
end
