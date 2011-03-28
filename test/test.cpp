/* Assumes util, job, remote, and mongoDeploy libraries have been built and installed in /usr/local/include and /usr/local/lib.
 * Compile as: g++ test.cpp -o test -I/opt/local/include -L/opt/local/lib -lboost_system-mt -lboost_filesystem-mt -lboost_thread-mt -lboost_serialization-mt -l10util -ljob -lremote -lmongoDeploy -lmongoclient -lpcre
 * Run as: `test` */

#include <mongoDeploy/mongoDeploy.h>

using namespace std;

static mongoDeploy::ReplicaSet startReplicaSet () {
	vector<remote::Host> hosts;
	hosts.push_back ("localhost");
	hosts.push_back ("localhost");
	hosts.push_back ("localhost");
	vector<mongoDeploy::RsMemberSpec> specs;
	specs.push_back (mongoDeploy::RsMemberSpec (program::Options(), mongo::BSONObj()));
	specs.push_back (mongoDeploy::RsMemberSpec (program::Options(), mongo::BSONObj()));
	specs.push_back (mongoDeploy::RsMemberSpec (program::Options(), BSON ("arbiterOnly" << true)));
	return mongoDeploy::startReplicaSet (hosts, specs);
}

int main (int argc, const char* argv[]) {
	boost::shared_ptr<boost::thread> th = remote::listen();
	mongoDeploy::ReplicaSet rs = startReplicaSet();
	cout << rs.nameActiveHosts() << endl;
	th->join();
}
