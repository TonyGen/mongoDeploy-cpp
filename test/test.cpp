/* Assumes util, job, remote, and mongoDeploy libraries have been built and installed in /usr/local/include and /usr/local/lib.
 * Compile as: g++ test.cpp -o test -I/opt/local/include -L/opt/local/lib -lboost_system-mt -lboost_filesystem-mt -lboost_thread-mt -lboost_serialization-mt -l10util -ljob -lremote -lmongoDeploy -lmongoclient -lpcre
 * Run as: `test` */

#include <mongoDeploy/mongoDeploy.h>
#include <remote/procedure.h>

using namespace std;

void testBsonObj () {
	mongo::BSONObj x = BSON ("a" << "hello" << "bb" << 42);
	string s = serialized (x);
	mongo::BSONObj y = deserialized<mongo::BSONObj> (s);
	assert (x == y);
	cout << x << endl;
	cout << s << endl;
	cout << y << endl;
}

int main (int argc, const char* argv[]) {
	testBsonObj();
}
