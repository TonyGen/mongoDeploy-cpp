libname = 'mongoDeploy'

lib = SharedLibrary (libname, Glob('*.cpp'),
	CCFLAGS = ['-g', '-rdynamic'],
	CPPPATH = ['.', '/usr/local/include'],
	LIBPATH = ['/usr/local/lib'],
	LIBS = Split ('mongoclient 10util 10remote boost_system-mt boost_thread-mt boost_filesystem-mt boost_serialization-mt') )

Alias ('install', '/usr/local')
Install ('/usr/local/lib', lib)
Install ('/usr/local/include/' + libname, Glob('*.h'))
