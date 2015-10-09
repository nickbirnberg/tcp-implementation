# -*- mode: ruby -*-
# vi: set ft=ruby :

Vagrant.configure(2) do |config|
  config.vm.box = "ubuntu/trusty64"

  config.vm.provision "shell", inline: <<-SHELL
    sudo apt-get update
    sudo apt-get install -y gcc g++ make gdb iperf tcpdump wget
  SHELL

  config.vm.define "sender" do |sender|
    sender.vm.network "private_network", ip: "10.66.66.10"
    sender.vm.hostname = "sendervm"
    sender.vm.provision "shell", run: "always", inline: <<-SHELL
      sudo tc qdisc del dev eth1 root 2>/dev/null
      sudo tc qdisc add dev eth1 root handle 1:0 netem delay 20ms loss 5%
      sudo tc qdisc add dev eth1 parent 1:1 handle 10: tbf rate 40Mbit burst 10mb latency 1ms
    SHELL
  end

  config.vm.define "receiver" do |sender|
    sender.vm.network "private_network", ip: "10.66.66.11"
    sender.vm.hostname = "receivervm"
  end
end
