sudo apt-get install -y curl

# install dev tools
sudo apt-get install -y libtool autoconf

# install grpc
cd ~/
git clone -b $(curl -L https://grpc.io/release) https://github.com/grpc/grpc
cd grpc
git submodule update --init

make
sudo make install

# install protobuf
cd third_party/protobuf
./autogen.sh
./configure
make
sudo make install

# This is required to run performance benchmarks
sudo apt-get install libgflags-dev


# install new cmake
cd $HOME
wget https://cmake.org/files/v3.10/cmake-3.10.0-Linux-x86_64.tar.gz
tar xf cmake-3.10.0-Linux-x86_64.tar.gz
echo 'export PATH=$HOME/cmake-3.10.0-Linux-x86_64/bin:$PATH' >> $HOME/.bashrc

# install GLOG
cd $HOME
wget https://github.com/google/glog/archive/v0.3.5.tar.gz
tar xf v0.3.5.tar.gz
cd glog-0.3.5
./configure
make
sudo make install



