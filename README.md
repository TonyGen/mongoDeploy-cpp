Install dependent library first
- [remote](https://github.com/TonyGen/remote-cpp)

Remove '-ccp' suffix when downloading
	git clone git://github.com/TonyGen/mongoDeploy-cpp.git mongoDeploy
	cd mongoDeploy

Build library `libmongoDeploy.a`
	scons

Install library in `/usr/local/lib` and header files in `/usr/local/include/mongoDeploy`
	sudo scons install
