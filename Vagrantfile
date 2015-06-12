Vagrant.configure(2) do |config|
  config.vm.box = "ubuntu/trusty64"
  config.vm.network "private_network", ip: "10.10.10.10"
  config.vm.synced_folder ".", "/vagrant",
    :nfs => /darwin|linux/ =~ RUBY_PLATFORM

  config.vm.provider "virtualbox" do |vb|
    vb.name = "epoll_server"
    vb.memory = 1024
    vb.customize ["modifyvm", :id, "--natdnshostresolver1", "on"]
  end
end
