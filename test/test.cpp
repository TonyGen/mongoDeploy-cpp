/* Assumes util, remote, and mongoDeploy libraries have been built and installed in /usr/local/include and /usr/local/lib.
 * Compile as: g++ test.cpp -o test -I/usr/local/include -L/usr/local/lib -lboost_system-mt -lboost_filesystem-mt -lboost_thread-mt -lboost_serialization-mt -l10util -lremote -lmongoDeploy -lmongoclient -lpcre
 * Run as: `test` */

#include <mongoDeploy/mongoDeploy.h>
#include <10util/io.h>

using namespace std;

void testBsonObj () {
	mongo::BSONObj x = BSON ("a" << "hello" << "bb" << 42);
	io::Code c = io::encode (x);
	mongo::BSONObj y = io::decode<mongo::BSONObj> (c);
	assert (x == y);
	cout << x << endl;
	cout << c << endl;
	cout << y << endl;
}

int main (int argc, const char* argv[]) {
	testBsonObj();
}
