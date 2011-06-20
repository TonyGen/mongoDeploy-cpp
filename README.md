Install dependent library first:

- [10remote](https://github.com/TonyGen/10remote-cpp)

Download and remove '-ccp' suffix:

	git clone git://github.com/TonyGen/mongoDeploy-cpp.git mongoDeploy
	cd mongoDeploy

Build library `libmongoDeploy.so`:

	scons

Install library in `/usr/local/lib` and header files in `/usr/local/include/mongoDeploy`:

	sudo scons install
