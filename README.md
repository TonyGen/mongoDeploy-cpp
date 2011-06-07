Install dependent library first:

- [remote](https://github.com/TonyGen/remote-cpp)

Download and remove '-ccp' suffix:

	git clone git://github.com/TonyGen/mongoDeploy-cpp.git mongoDeploy
	cd mongoDeploy

Build library `libmongoDeploy.so`:

	scons

Install library in `/usr/local/lib` and header files in `/usr/local/include/mongoDeploy`:

	sudo scons install
